/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "config.h"
#include "fileops.h"
#include "filebuf.h"
#include "buffer.h"
#include "git2/config.h"
#include "git2/types.h"
#include "strmap.h"

#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

GIT__USE_STRMAP;

typedef struct cvar_t {
	struct cvar_t *next;
	git_config_entry *entry;
} cvar_t;

#define CVAR_LIST_HEAD(list) ((list)->head)

#define CVAR_LIST_TAIL(list) ((list)->tail)

#define CVAR_LIST_NEXT(var) ((var)->next)

#define CVAR_LIST_EMPTY(list) ((list)->head == NULL)

#define CVAR_LIST_APPEND(list, var) do {\
	if (CVAR_LIST_EMPTY(list)) {\
		CVAR_LIST_HEAD(list) = CVAR_LIST_TAIL(list) = var;\
	} else {\
		CVAR_LIST_NEXT(CVAR_LIST_TAIL(list)) = var;\
		CVAR_LIST_TAIL(list) = var;\
	}\
} while(0)

#define CVAR_LIST_REMOVE_HEAD(list) do {\
	CVAR_LIST_HEAD(list) = CVAR_LIST_NEXT(CVAR_LIST_HEAD(list));\
} while(0)

#define CVAR_LIST_REMOVE_AFTER(var) do {\
	CVAR_LIST_NEXT(var) = CVAR_LIST_NEXT(CVAR_LIST_NEXT(var));\
} while(0)

#define CVAR_LIST_FOREACH(list, iter)\
	for ((iter) = CVAR_LIST_HEAD(list);\
		 (iter) != NULL;\
		 (iter) = CVAR_LIST_NEXT(iter))

/*
 * Inspired by the FreeBSD functions
 */
#define CVAR_LIST_FOREACH_SAFE(start, iter, tmp)\
	for ((iter) = CVAR_LIST_HEAD(vars);\
		 (iter) && (((tmp) = CVAR_LIST_NEXT(iter) || 1));\
		 (iter) = (tmp))

typedef struct {
	git_config_file parent;

	git_strmap *values;

	struct {
		git_buf buffer;
		char *read_ptr;
		int line_number;
		int eof;
	} reader;

	char *file_path;
} diskfile_backend;

static int config_parse(diskfile_backend *cfg_file, unsigned int level);
static int parse_variable(diskfile_backend *cfg, char **var_name, char **var_value);
static int config_write(diskfile_backend *cfg, const char *key, const regex_t *preg, const char *value);
static char *escape_value(const char *ptr);

static void set_parse_error(diskfile_backend *backend, int col, const char *error_str)
{
	giterr_set(GITERR_CONFIG, "Failed to parse config file: %s (in %s:%d, column %d)",
		error_str, backend->file_path, backend->reader.line_number, col);
}

static void cvar_free(cvar_t *var)
{
	if (var == NULL)
		return;

	git__free((char*)var->entry->name);
	git__free((char *)var->entry->value);
	git__free(var->entry);
	git__free(var);
}

/* Take something the user gave us and make it nice for our hash function */
static int normalize_name(const char *in, char **out)
{
	char *name, *fdot, *ldot;

	assert(in && out);

	name = git__strdup(in);
	GITERR_CHECK_ALLOC(name);

	fdot = strchr(name, '.');
	ldot = strrchr(name, '.');

	if (fdot == NULL || ldot == NULL) {
		git__free(name);
		giterr_set(GITERR_CONFIG,
			"Invalid variable name: '%s'", in);
		return -1;
	}

	/* Downcase up to the first dot and after the last one */
	git__strntolower(name, fdot - name);
	git__strtolower(ldot);

	*out = name;
	return 0;
}

static void free_vars(git_strmap *values)
{
	cvar_t *var = NULL;

	if (values == NULL)
		return;

	git_strmap_foreach_value(values, var,
		while (var != NULL) {
			cvar_t *next = CVAR_LIST_NEXT(var);
			cvar_free(var);
			var = next;
		});

	git_strmap_free(values);
}

static int config_open(git_config_file *cfg, unsigned int level)
{
	int res;
	diskfile_backend *b = (diskfile_backend *)cfg;

	b->values = git_strmap_alloc();
	GITERR_CHECK_ALLOC(b->values);

	git_buf_init(&b->reader.buffer, 0);
	res = git_futils_readbuffer(&b->reader.buffer, b->file_path);

	/* It's fine if the file doesn't exist */
	if (res == GIT_ENOTFOUND)
		return 0;

	if (res < 0 || config_parse(b, level) <  0) {
		free_vars(b->values);
		b->values = NULL;
		git_buf_free(&b->reader.buffer);
		return -1;
	}

	git_buf_free(&b->reader.buffer);
	return 0;
}

static void backend_free(git_config_file *_backend)
{
	diskfile_backend *backend = (diskfile_backend *)_backend;

	if (backend == NULL)
		return;

	git__free(backend->file_path);
	free_vars(backend->values);
	git__free(backend);
}

static int file_foreach(
	git_config_file *backend,
	const char *regexp,
	int (*fn)(const git_config_entry *, void *),
	void *data)
{
	diskfile_backend *b = (diskfile_backend *)backend;
	cvar_t *var, *next_var;
	const char *key;
	regex_t regex;
	int result = 0;

	if (!b->values)
		return 0;

	if (regexp != NULL) {
		if ((result = regcomp(&regex, regexp, REG_EXTENDED)) < 0) {
			giterr_set_regex(&regex, result);
			regfree(&regex);
			return -1;
		}
	}

	git_strmap_foreach(b->values, key, var,
		for (; var != NULL; var = next_var) {
			next_var = CVAR_LIST_NEXT(var);

			/* skip non-matching keys if regexp was provided */
			if (regexp && regexec(&regex, key, 0, NULL, 0) != 0)
				continue;

			/* abort iterator on non-zero return value */
			if (fn(var->entry, data)) {
				giterr_clear();
				result = GIT_EUSER;
				goto cleanup;
			}
		}
	);

cleanup:
	if (regexp != NULL)
		regfree(&regex);

	return result;
}

static int config_set(git_config_file *cfg, const char *name, const char *value)
{
	cvar_t *var = NULL, *old_var;
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key, *esc_value = NULL;
	khiter_t pos;
	int rval, ret;

	if (normalize_name(name, &key) < 0)
		return -1;

	/*
	 * Try to find it in the existing values and update it if it
	 * only has one value.
	 */
	pos = git_strmap_lookup_index(b->values, key);
	if (git_strmap_valid_index(b->values, pos)) {
		cvar_t *existing = git_strmap_value_at(b->values, pos);
		char *tmp = NULL;

		git__free(key);

		if (existing->next != NULL) {
			giterr_set(GITERR_CONFIG, "Multivar incompatible with simple set");
			return -1;
		}

		/* don't update if old and new values already match */
		if ((!existing->entry->value && !value) ||
			(existing->entry->value && value && !strcmp(existing->entry->value, value)))
			return 0;

		if (value) {
			tmp = git__strdup(value);
			GITERR_CHECK_ALLOC(tmp);
			esc_value = escape_value(value);
			GITERR_CHECK_ALLOC(esc_value);
		}

		git__free((void *)existing->entry->value);
		existing->entry->value = tmp;

		ret = config_write(b, existing->entry->name, NULL, esc_value);

		git__free(esc_value);
		return ret;
	}

	var = git__malloc(sizeof(cvar_t));
	GITERR_CHECK_ALLOC(var);
	memset(var, 0x0, sizeof(cvar_t));
	var->entry = git__malloc(sizeof(git_config_entry));
	GITERR_CHECK_ALLOC(var->entry);
	memset(var->entry, 0x0, sizeof(git_config_entry));

	var->entry->name = key;
	var->entry->value = NULL;

	if (value) {
		var->entry->value = git__strdup(value);
		GITERR_CHECK_ALLOC(var->entry->value);
		esc_value = escape_value(value);
		GITERR_CHECK_ALLOC(esc_value);
	}

	if (config_write(b, key, NULL, esc_value) < 0) {
		git__free(esc_value);
		cvar_free(var);
		return -1;
	}

	git__free(esc_value);
	git_strmap_insert2(b->values, key, var, old_var, rval);
	if (rval < 0)
		return -1;
	if (old_var != NULL)
		cvar_free(old_var);

	return 0;
}

/*
 * Internal function that actually gets the value in string form
 */
static int config_get(git_config_file *cfg, const char *name, const git_config_entry **out)
{
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key;
	khiter_t pos;

	if (normalize_name(name, &key) < 0)
		return -1;

	pos = git_strmap_lookup_index(b->values, key);
	git__free(key);

	/* no error message; the config system will write one */
	if (!git_strmap_valid_index(b->values, pos))
		return GIT_ENOTFOUND;

	*out = ((cvar_t *)git_strmap_value_at(b->values, pos))->entry;

	return 0;
}

static int config_get_multivar(
	git_config_file *cfg,
	const char *name,
	const char *regex_str,
	int (*fn)(const git_config_entry *, void *),
	void *data)
{
	cvar_t *var;
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key;
	khiter_t pos;

	if (normalize_name(name, &key) < 0)
		return -1;

	pos = git_strmap_lookup_index(b->values, key);
	git__free(key);

	if (!git_strmap_valid_index(b->values, pos))
		return GIT_ENOTFOUND;

	var = git_strmap_value_at(b->values, pos);

	if (regex_str != NULL) {
		regex_t regex;
		int result;

		/* regex matching; build the regex */
		result = regcomp(&regex, regex_str, REG_EXTENDED);
		if (result < 0) {
			giterr_set_regex(&regex, result);
			regfree(&regex);
			return -1;
		}

		/* and throw the callback only on the variables that
		 * match the regex */
		do {
			if (regexec(&regex, var->entry->value, 0, NULL, 0) == 0) {
				/* early termination by the user is not an error;
				 * just break and return successfully */
				if (fn(var->entry, data) < 0)
					break;
			}

			var = var->next;
		} while (var != NULL);
		regfree(&regex);
	} else {
		/* no regex; go through all the variables */
		do {
			/* early termination by the user is not an error;
			 * just break and return successfully */
			if (fn(var->entry, data) < 0)
				break;

			var = var->next;
		} while (var != NULL);
	}

	return 0;
}

static int config_set_multivar(
	git_config_file *cfg, const char *name, const char *regexp, const char *value)
{
	int replaced = 0;
	cvar_t *var, *newvar;
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key;
	regex_t preg;
	int result;
	khiter_t pos;

	assert(regexp);

	if (normalize_name(name, &key) < 0)
		return -1;

	pos = git_strmap_lookup_index(b->values, key);
	if (!git_strmap_valid_index(b->values, pos)) {
		git__free(key);
		return GIT_ENOTFOUND;
	}

	var = git_strmap_value_at(b->values, pos);

	result = regcomp(&preg, regexp, REG_EXTENDED);
	if (result < 0) {
		git__free(key);
		giterr_set_regex(&preg, result);
		regfree(&preg);
		return -1;
	}

	for (;;) {
		if (regexec(&preg, var->entry->value, 0, NULL, 0) == 0) {
			char *tmp = git__strdup(value);
			GITERR_CHECK_ALLOC(tmp);

			git__free((void *)var->entry->value);
			var->entry->value = tmp;
			replaced = 1;
		}

		if (var->next == NULL)
			break;

		var = var->next;
	}

	/* If we've reached the end of the variables and we haven't found it yet, we need to append it */
	if (!replaced) {
		newvar = git__malloc(sizeof(cvar_t));
		GITERR_CHECK_ALLOC(newvar);
		memset(newvar, 0x0, sizeof(cvar_t));
		newvar->entry = git__malloc(sizeof(git_config_entry));
		GITERR_CHECK_ALLOC(newvar->entry);
		memset(newvar->entry, 0x0, sizeof(git_config_entry));

		newvar->entry->name = git__strdup(var->entry->name);
		GITERR_CHECK_ALLOC(newvar->entry->name);

		newvar->entry->value = git__strdup(value);
		GITERR_CHECK_ALLOC(newvar->entry->value);

		newvar->entry->level = var->entry->level;

		var->next = newvar;
	}

	result = config_write(b, key, &preg, value);

	git__free(key);
	regfree(&preg);

	return result;
}

static int config_delete(git_config_file *cfg, const char *name)
{
	cvar_t *var;
	diskfile_backend *b = (diskfile_backend *)cfg;
	char *key;
	int result;
	khiter_t pos;

	if (normalize_name(name, &key) < 0)
		return -1;

	pos = git_strmap_lookup_index(b->values, key);
	git__free(key);

	if (!git_strmap_valid_index(b->values, pos)) {
		giterr_set(GITERR_CONFIG, "Could not find key '%s' to delete", name);
		return GIT_ENOTFOUND;
	}

	var = git_strmap_value_at(b->values, pos);

	if (var->next != NULL) {
		giterr_set(GITERR_CONFIG, "Cannot delete multivar with a single delete");
		return -1;
	}

	git_strmap_delete_at(b->values, pos);

	result = config_write(b, var->entry->name, NULL, NULL);

	cvar_free(var);
	return result;
}

int git_config_file__ondisk(git_config_file **out, const char *path)
{
	diskfile_backend *backend;

	backend = git__malloc(sizeof(diskfile_backend));
	GITERR_CHECK_ALLOC(backend);

	memset(backend, 0x0, sizeof(diskfile_backend));

	backend->file_path = git__strdup(path);
	GITERR_CHECK_ALLOC(backend->file_path);

	backend->parent.open = config_open;
	backend->parent.get = config_get;
	backend->parent.get_multivar = config_get_multivar;
	backend->parent.set = config_set;
	backend->parent.set_multivar = config_set_multivar;
	backend->parent.del = config_delete;
	backend->parent.foreach = file_foreach;
	backend->parent.free = backend_free;

	*out = (git_config_file *)backend;

	return 0;
}

static int cfg_getchar_raw(diskfile_backend *cfg)
{
	int c;

	c = *cfg->reader.read_ptr++;

	/*
	Win 32 line breaks: if we find a \r\n sequence,
	return only the \n as a newline
	*/
	if (c == '\r' && *cfg->reader.read_ptr == '\n') {
		cfg->reader.read_ptr++;
		c = '\n';
	}

	if (c == '\n')
		cfg->reader.line_number++;

	if (c == 0) {
		cfg->reader.eof = 1;
		c = '\n';
	}

	return c;
}

#define SKIP_WHITESPACE (1 << 1)
#define SKIP_COMMENTS (1 << 2)

static int cfg_getchar(diskfile_backend *cfg_file, int flags)
{
	const int skip_whitespace = (flags & SKIP_WHITESPACE);
	const int skip_comments = (flags & SKIP_COMMENTS);
	int c;

	assert(cfg_file->reader.read_ptr);

	do c = cfg_getchar_raw(cfg_file);
	while (skip_whitespace && git__isspace(c) &&
	       !cfg_file->reader.eof);

	if (skip_comments && (c == '#' || c == ';')) {
		do c = cfg_getchar_raw(cfg_file);
		while (c != '\n');
	}

	return c;
}

/*
 * Read the next char, but don't move the reading pointer.
 */
static int cfg_peek(diskfile_backend *cfg, int flags)
{
	void *old_read_ptr;
	int old_lineno, old_eof;
	int ret;

	assert(cfg->reader.read_ptr);

	old_read_ptr = cfg->reader.read_ptr;
	old_lineno = cfg->reader.line_number;
	old_eof = cfg->reader.eof;

	ret = cfg_getchar(cfg, flags);

	cfg->reader.read_ptr = old_read_ptr;
	cfg->reader.line_number = old_lineno;
	cfg->reader.eof = old_eof;

	return ret;
}

/*
 * Read and consume a line, returning it in newly-allocated memory.
 */
static char *cfg_readline(diskfile_backend *cfg, bool skip_whitespace)
{
	char *line = NULL;
	char *line_src, *line_end;
	size_t line_len;

	line_src = cfg->reader.read_ptr;

	if (skip_whitespace) {
		/* Skip empty empty lines */
		while (git__isspace(*line_src))
			++line_src;
	}

	line_end = strchr(line_src, '\n');

	/* no newline at EOF */
	if (line_end == NULL)
		line_end = strchr(line_src, 0);

	line_len = line_end - line_src;

	line = git__malloc(line_len + 1);
	if (line == NULL)
		return NULL;

	memcpy(line, line_src, line_len);

	do line[line_len] = '\0';
	while (line_len-- > 0 && git__isspace(line[line_len]));

	if (*line_end == '\n')
		line_end++;

	if (*line_end == '\0')
		cfg->reader.eof = 1;

	cfg->reader.line_number++;
	cfg->reader.read_ptr = line_end;

	return line;
}

/*
 * Consume a line, without storing it anywhere
 */
static void cfg_consume_line(diskfile_backend *cfg)
{
	char *line_start, *line_end;

	line_start = cfg->reader.read_ptr;
	line_end = strchr(line_start, '\n');
	/* No newline at EOF */
	if(line_end == NULL){
		line_end = strchr(line_start, '\0');
	}

	if (*line_end == '\n')
		line_end++;

	if (*line_end == '\0')
		cfg->reader.eof = 1;

	cfg->reader.line_number++;
	cfg->reader.read_ptr = line_end;
}

GIT_INLINE(int) config_keychar(int c)
{
	return isalnum(c) || c == '-';
}

static int parse_section_header_ext(diskfile_backend *cfg, const char *line, const char *base_name, char **section_name)
{
	int c, rpos;
	char *first_quote, *last_quote;
	git_buf buf = GIT_BUF_INIT;
	int quote_marks;
	/*
	 * base_name is what came before the space. We should be at the
	 * first quotation mark, except for now, line isn't being kept in
	 * sync so we only really use it to calculate the length.
	 */

	first_quote = strchr(line, '"');
	last_quote = strrchr(line, '"');

	if (last_quote - first_quote == 0) {
		set_parse_error(cfg, 0, "Missing closing quotation mark in section header");
		return -1;
	}

	git_buf_grow(&buf, strlen(base_name) + last_quote - first_quote + 2);
	git_buf_printf(&buf, "%s.", base_name);

	rpos = 0;
	quote_marks = 0;

	line = first_quote;
	c = line[rpos++];

	/*
	 * At the end of each iteration, whatever is stored in c will be
	 * added to the string. In case of error, jump to out
	 */
	do {
		if (quote_marks == 2) {
			set_parse_error(cfg, rpos, "Unexpected text after closing quotes");
			git_buf_free(&buf);
			return -1;
		}

		switch (c) {
		case '"':
			++quote_marks;
			continue;

		case '\\':
			c = line[rpos++];

			switch (c) {
			case '"':
			case '\\':
				break;

			default:
				set_parse_error(cfg, rpos, "Unsupported escape sequence");
				git_buf_free(&buf);
				return -1;
			}

		default:
			break;
		}

		git_buf_putc(&buf, c);
	} while ((c = line[rpos++]) != ']');

	*section_name = git_buf_detach(&buf);
	return 0;
}

static int parse_section_header(diskfile_backend *cfg, char **section_out)
{
	char *name, *name_end;
	int name_length, c, pos;
	int result;
	char *line;

	line = cfg_readline(cfg, true);
	if (line == NULL)
		return -1;

	/* find the end of the variable's name */
	name_end = strchr(line, ']');
	if (name_end == NULL) {
		git__free(line);
		set_parse_error(cfg, 0, "Missing ']' in section header");
		return -1;
	}

	name = (char *)git__malloc((size_t)(name_end - line) + 1);
	GITERR_CHECK_ALLOC(name);

	name_length = 0;
	pos = 0;

	/* Make sure we were given a section header */
	c = line[pos++];
	assert(c == '[');

	c = line[pos++];

	do {
		if (git__isspace(c)){
			name[name_length] = '\0';
			result = parse_section_header_ext(cfg, line, name, section_out);
			git__free(line);
			git__free(name);
			return result;
		}

		if (!config_keychar(c) && c != '.') {
			set_parse_error(cfg, pos, "Unexpected character in header");
			goto fail_parse;
		}

		name[name_length++] = (char) tolower(c);

	} while ((c = line[pos++]) != ']');

	if (line[pos - 1] != ']') {
		set_parse_error(cfg, pos, "Unexpected end of file");
		goto fail_parse;
	}

	git__free(line);

	name[name_length] = 0;
	*section_out = name;

	return 0;

fail_parse:
	git__free(line);
	git__free(name);
	return -1;
}

static int skip_bom(diskfile_backend *cfg)
{
	static const char utf8_bom[] = { '\xef', '\xbb', '\xbf' };

	if (cfg->reader.buffer.size < sizeof(utf8_bom))
		return 0;

	if (memcmp(cfg->reader.read_ptr, utf8_bom, sizeof(utf8_bom)) == 0)
		cfg->reader.read_ptr += sizeof(utf8_bom);

	/* TODO: the reference implementation does pretty stupid
		shit with the BoM
	*/

	return 0;
}

/*
	(* basic types *)
	digit = "0".."9"
	integer = digit { digit }
	alphabet = "a".."z" + "A" .. "Z"

	section_char = alphabet | "." | "-"
	extension_char = (* any character except newline *)
	any_char = (* any character *)
	variable_char = "alphabet" | "-"


	(* actual grammar *)
	config = { section }

	section = header { definition }

	header = "[" section [subsection | subsection_ext] "]"

	subsection = "." section
	subsection_ext = "\"" extension "\""

	section = section_char { section_char }
	extension = extension_char { extension_char }

	definition = variable_name ["=" variable_value] "\n"

	variable_name = variable_char { variable_char }
	variable_value = string | boolean | integer

	string = quoted_string | plain_string
	quoted_string = "\"" plain_string "\""
	plain_string = { any_char }

	boolean = boolean_true | boolean_false
	boolean_true = "yes" | "1" | "true" | "on"
	boolean_false = "no" | "0" | "false" | "off"
*/

static int strip_comments(char *line, int in_quotes)
{
	int quote_count = in_quotes;
	char *ptr;

	for (ptr = line; *ptr; ++ptr) {
		if (ptr[0] == '"' && ptr > line && ptr[-1] != '\\')
			quote_count++;

		if ((ptr[0] == ';' || ptr[0] == '#') && (quote_count % 2) == 0) {
			ptr[0] = '\0';
			break;
		}
	}

	/* skip any space at the end */
	if (git__isspace(ptr[-1])) {
		ptr--;
	}
	ptr[0] = '\0';

	return quote_count;
}

static int config_parse(diskfile_backend *cfg_file, unsigned int level)
{
	int c;
	char *current_section = NULL;
	char *var_name;
	char *var_value;
	cvar_t *var, *existing;
	git_buf buf = GIT_BUF_INIT;
	int result = 0;
	khiter_t pos;

	/* Initialize the reading position */
	cfg_file->reader.read_ptr = cfg_file->reader.buffer.ptr;
	cfg_file->reader.eof = 0;

	/* If the file is empty, there's nothing for us to do */
	if (*cfg_file->reader.read_ptr == '\0')
		return 0;

	skip_bom(cfg_file);

	while (result == 0 && !cfg_file->reader.eof) {

		c = cfg_peek(cfg_file, SKIP_WHITESPACE);

		switch (c) {
		case '\n': /* EOF when peeking, set EOF in the reader to exit the loop */
			cfg_file->reader.eof = 1;
			break;

		case '[': /* section header, new section begins */
			git__free(current_section);
			current_section = NULL;
			result = parse_section_header(cfg_file, &current_section);
			break;

		case ';':
		case '#':
			cfg_consume_line(cfg_file);
			break;

		default: /* assume variable declaration */
			result = parse_variable(cfg_file, &var_name, &var_value);
			if (result < 0)
				break;

			var = git__malloc(sizeof(cvar_t));
			GITERR_CHECK_ALLOC(var);
			memset(var, 0x0, sizeof(cvar_t));
			var->entry = git__malloc(sizeof(git_config_entry));
			GITERR_CHECK_ALLOC(var->entry);
			memset(var->entry, 0x0, sizeof(git_config_entry));

			git__strtolower(var_name);
			git_buf_printf(&buf, "%s.%s", current_section, var_name);
			git__free(var_name);

			if (git_buf_oom(&buf))
				return -1;

			var->entry->name = git_buf_detach(&buf);
			var->entry->value = var_value;
			var->entry->level = level;

			/* Add or append the new config option */
			pos = git_strmap_lookup_index(cfg_file->values, var->entry->name);
			if (!git_strmap_valid_index(cfg_file->values, pos)) {
				git_strmap_insert(cfg_file->values, var->entry->name, var, result);
				if (result < 0)
					break;
				result = 0;
			} else {
				existing = git_strmap_value_at(cfg_file->values, pos);
				while (existing->next != NULL) {
					existing = existing->next;
				}
				existing->next = var;
			}

			break;
		}
	}

	git__free(current_section);
	return result;
}

static int write_section(git_filebuf *file, const char *key)
{
	int result;
	const char *dot;
	git_buf buf = GIT_BUF_INIT;

	/* All of this just for [section "subsection"] */
	dot = strchr(key, '.');
	git_buf_putc(&buf, '[');
	if (dot == NULL) {
		git_buf_puts(&buf, key);
	} else {
		char *escaped;
		git_buf_put(&buf, key, dot - key);
		escaped = escape_value(dot + 1);
		GITERR_CHECK_ALLOC(escaped);
		git_buf_printf(&buf, " \"%s\"", escaped);
		git__free(escaped);
	}
	git_buf_puts(&buf, "]\n");

	if (git_buf_oom(&buf))
		return -1;

	result = git_filebuf_write(file, git_buf_cstr(&buf), buf.size);
	git_buf_free(&buf);

	return result;
}

/*
 * This is pretty much the parsing, except we write out anything we don't have
 */
static int config_write(diskfile_backend *cfg, const char *key, const regex_t *preg, const char* value)
{
	int result, c;
	int section_matches = 0, last_section_matched = 0, preg_replaced = 0, write_trailer = 0;
	const char *pre_end = NULL, *post_start = NULL, *data_start;
	char *current_section = NULL, *section, *name, *ldot;
	git_filebuf file = GIT_FILEBUF_INIT;

	/* We need to read in our own config file */
	result = git_futils_readbuffer(&cfg->reader.buffer, cfg->file_path);

	/* Initialise the reading position */
	if (result == GIT_ENOTFOUND) {
		cfg->reader.read_ptr = NULL;
		cfg->reader.eof = 1;
		data_start = NULL;
		git_buf_clear(&cfg->reader.buffer);
	} else if (result == 0) {
		cfg->reader.read_ptr = cfg->reader.buffer.ptr;
		cfg->reader.eof = 0;
		data_start = cfg->reader.read_ptr;
	} else {
		return -1; /* OS error when reading the file */
	}

	/* Lock the file */
	if (git_filebuf_open(&file, cfg->file_path, 0) < 0)
		return -1;

	skip_bom(cfg);
	ldot = strrchr(key, '.');
	name = ldot + 1;
	section = git__strndup(key, ldot - key);

	while (!cfg->reader.eof) {
		c = cfg_peek(cfg, SKIP_WHITESPACE);

		if (c == '\0') { /* We've arrived at the end of the file */
			break;

		} else if (c == '[') { /* section header, new section begins */
			/*
			 * We set both positions to the current one in case we
			 * need to add a variable to the end of a section. In that
			 * case, we want both variables to point just before the
			 * new section. If we actually want to replace it, the
			 * default case will take care of updating them.
			 */
			pre_end = post_start = cfg->reader.read_ptr;

			git__free(current_section);
			current_section = NULL;
			if (parse_section_header(cfg, &current_section) < 0)
				goto rewrite_fail;

			/* Keep track of when it stops matching */
			last_section_matched = section_matches;
			section_matches = !strcmp(current_section, section);
		}

		else if (c == ';' || c == '#') {
			cfg_consume_line(cfg);
		}

		else {
			/*
			 * If the section doesn't match, but the last section did,
			 * it means we need to add a variable (so skip the line
			 * otherwise). If both the section and name match, we need
			 * to overwrite the variable (so skip the line
			 * otherwise). pre_end needs to be updated each time so we
			 * don't loose that information, but we only need to
			 * update post_start if we're going to use it in this
			 * iteration.
			 */
			if (!section_matches) {
				if (!last_section_matched) {
					cfg_consume_line(cfg);
					continue;
				}
			} else {
				int has_matched = 0;
				char *var_name, *var_value;

				pre_end = cfg->reader.read_ptr;
				if (parse_variable(cfg, &var_name, &var_value) < 0)
					goto rewrite_fail;

				/* First try to match the name of the variable */
				if (strcasecmp(name, var_name) == 0)
					has_matched = 1;

				/* If the name matches, and we have a regex to match the
				 * value, try to match it */
				if (has_matched && preg != NULL)
					has_matched = (regexec(preg, var_value, 0, NULL, 0) == 0);

				git__free(var_name);
				git__free(var_value);

				/* if there is no match, keep going */
				if (!has_matched)
					continue;

				post_start = cfg->reader.read_ptr;
			}

			/* We've found the variable we wanted to change, so
			 * write anything up to it */
			git_filebuf_write(&file, data_start, pre_end - data_start);
			preg_replaced = 1;

			/* Then replace the variable. If the value is NULL, it
			 * means we want to delete it, so don't write anything. */
			if (value != NULL) {
				git_filebuf_printf(&file, "\t%s = %s\n", name, value);
			}

			/* multiline variable? we need to keep reading lines to match */
			if (preg != NULL) {
				data_start = post_start;
				continue;
			}

			write_trailer = 1;
			break; /* break from the loop */
		}
	}

	/*
	 * Being here can mean that
	 *
	 * 1) our section is the last one in the file and we're
	 * adding a variable
	 *
	 * 2) we didn't find a section for us so we need to create it
	 * ourselves.
	 *
	 * 3) we're setting a multivar with a regex, which means we
	 * continue to search for matching values
	 *
	 * In the last case, if we've already replaced a value, we
	 * want to write the rest of the file. Otherwise we need to write
	 * out the whole file and then the new variable.
	 */
	if (write_trailer) {
		/* Write out rest of the file */
		git_filebuf_write(&file, post_start, cfg->reader.buffer.size - (post_start - data_start));
	} else {
		if (preg_replaced) {
			git_filebuf_printf(&file, "\n%s", data_start);
		} else {
			git_filebuf_write(&file, cfg->reader.buffer.ptr, cfg->reader.buffer.size);

			/* And now if we just need to add a variable */
			if (!section_matches && write_section(&file, section) < 0)
				goto rewrite_fail;

			/* Sanity check: if we are here, and value is NULL, that means that somebody
			 * touched the config file after our intial read. We should probably assert()
			 * this, but instead we'll handle it gracefully with an error. */
			if (value == NULL) {
				giterr_set(GITERR_CONFIG,
					"Race condition when writing a config file (a cvar has been removed)");
				goto rewrite_fail;
			}

			/* If we are here, there is at least a section line */
			if (*(cfg->reader.buffer.ptr + cfg->reader.buffer.size - 1) != '\n')
				git_filebuf_write(&file, "\n", 1);

			git_filebuf_printf(&file, "\t%s = %s\n", name, value);
		}
	}

	git__free(section);
	git__free(current_section);

	result = git_filebuf_commit(&file, GIT_CONFIG_FILE_MODE);
	git_buf_free(&cfg->reader.buffer);
	return result;

rewrite_fail:
	git__free(section);
	git__free(current_section);

	git_filebuf_cleanup(&file);
	git_buf_free(&cfg->reader.buffer);
	return -1;
}

static const char *escapes = "ntb\"\\";
static const char *escaped = "\n\t\b\"\\";

/* Escape the values to write them to the file */
static char *escape_value(const char *ptr)
{
	git_buf buf = GIT_BUF_INIT;
	size_t len;
	const char *esc;

	assert(ptr);

	len = strlen(ptr);
	git_buf_grow(&buf, len);

	while (*ptr != '\0') {
		if ((esc = strchr(escaped, *ptr)) != NULL) {
			git_buf_putc(&buf, '\\');
			git_buf_putc(&buf, escapes[esc - escaped]);
		} else {
			git_buf_putc(&buf, *ptr);
		}
		ptr++;
	}

	if (git_buf_oom(&buf)) {
		git_buf_free(&buf);
		return NULL;
	}

	return git_buf_detach(&buf);
}

/* '\"' -> '"' etc */
static char *fixup_line(const char *ptr, int quote_count)
{
	char *str = git__malloc(strlen(ptr) + 1);
	char *out = str, *esc;

	if (str == NULL)
		return NULL;

	while (*ptr != '\0') {
		if (*ptr == '"') {
			quote_count++;
		} else if (*ptr != '\\') {
			*out++ = *ptr;
		} else {
			/* backslash, check the next char */
			ptr++;
			/* if we're at the end, it's a multiline, so keep the backslash */
			if (*ptr == '\0') {
				*out++ = '\\';
				goto out;
			}
			if ((esc = strchr(escapes, *ptr)) != NULL) {
				*out++ = escaped[esc - escapes];
			} else {
				git__free(str);
				giterr_set(GITERR_CONFIG, "Invalid escape at %s", ptr);
				return NULL;
			}
		}
		ptr++;
	}

out:
	*out = '\0';

	return str;
}

static int is_multiline_var(const char *str)
{
	const char *end = str + strlen(str);
	return (end > str) && (end[-1] == '\\');
}

static int parse_multiline_variable(diskfile_backend *cfg, git_buf *value, int in_quotes)
{
	char *line = NULL, *proc_line = NULL;
	int quote_count;

	/* Check that the next line exists */
	line = cfg_readline(cfg, false);
	if (line == NULL)
		return -1;

	/* We've reached the end of the file, there is input missing */
	if (line[0] == '\0') {
		set_parse_error(cfg, 0, "Unexpected end of file while parsing multine var");
		git__free(line);
		return -1;
	}

	quote_count = strip_comments(line, !!in_quotes);

	/* If it was just a comment, pretend it didn't exist */
	if (line[0] == '\0') {
		git__free(line);
		return parse_multiline_variable(cfg, value, quote_count);
		/* TODO: unbounded recursion. This **could** be exploitable */
	}

	/* Drop the continuation character '\': to closely follow the UNIX
	 * standard, this character **has** to be last one in the buf, with
	 * no whitespace after it */
	assert(is_multiline_var(value->ptr));
	git_buf_truncate(value, git_buf_len(value) - 1);

	proc_line = fixup_line(line, in_quotes);
	if (proc_line == NULL) {
		git__free(line);
		return -1;
	}
	/* add this line to the multiline var */
	git_buf_puts(value, proc_line);
	git__free(line);
	git__free(proc_line);

	/*
	 * If we need to continue reading the next line, let's just
	 * keep putting stuff in the buffer
	 */
	if (is_multiline_var(value->ptr))
		return parse_multiline_variable(cfg, value, quote_count);

	return 0;
}

static int parse_variable(diskfile_backend *cfg, char **var_name, char **var_value)
{
	const char *var_end = NULL;
	const char *value_start = NULL;
	char *line;
	int quote_count;

	line = cfg_readline(cfg, true);
	if (line == NULL)
		return -1;

	quote_count = strip_comments(line, 0);

	var_end = strchr(line, '=');

	if (var_end == NULL)
		var_end = strchr(line, '\0');
	else
		value_start = var_end + 1;

	do var_end--;
	while (git__isspace(*var_end));

	*var_name = git__strndup(line, var_end - line + 1);
	GITERR_CHECK_ALLOC(*var_name);

	/* If there is no value, boolean true is assumed */
	*var_value = NULL;

	/*
	 * Now, let's try to parse the value
	 */
	if (value_start != NULL) {
		while (git__isspace(value_start[0]))
			value_start++;

		if (is_multiline_var(value_start)) {
			git_buf multi_value = GIT_BUF_INIT;
			char *proc_line = fixup_line(value_start, 0);
			GITERR_CHECK_ALLOC(proc_line);
			git_buf_puts(&multi_value, proc_line);
			git__free(proc_line);
			if (parse_multiline_variable(cfg, &multi_value, quote_count) < 0 || git_buf_oom(&multi_value)) {
				git__free(*var_name);
				git__free(line);
				git_buf_free(&multi_value);
				return -1;
			}

			*var_value = git_buf_detach(&multi_value);

		}
		else if (value_start[0] != '\0') {
			*var_value = fixup_line(value_start, 0);
			GITERR_CHECK_ALLOC(*var_value);
		}

	}

	git__free(line);
	return 0;
}
