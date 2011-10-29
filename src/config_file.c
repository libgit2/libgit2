/*
 * Copyright (C) 2009-2011 the libgit2 contributors
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


#include <ctype.h>

typedef struct cvar_t {
	struct cvar_t *next;
	char *section;
	char *name;
	char *value;
} cvar_t;

typedef struct {
	struct cvar_t *head;
	struct cvar_t *tail;
} cvar_t_list;

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

	cvar_t_list var_list;

	struct {
		git_fbuffer buffer;
		char *read_ptr;
		int line_number;
		int eof;
	} reader;

	char *file_path;
} diskfile_backend;

static int config_parse(diskfile_backend *cfg_file);
static int parse_variable(diskfile_backend *cfg, char **var_name, char **var_value);
static int config_write(diskfile_backend *cfg, cvar_t *var);

static void cvar_free(cvar_t *var)
{
	if (var == NULL)
		return;

	git__free(var->section);
	git__free(var->name);
	git__free(var->value);
	git__free(var);
}

static void cvar_list_free(cvar_t_list *list)
{
	cvar_t *cur;

	while (!CVAR_LIST_EMPTY(list)) {
		cur = CVAR_LIST_HEAD(list);
		CVAR_LIST_REMOVE_HEAD(list);
		cvar_free(cur);
	}
}

/*
 * Compare according to the git rules. Section contains the section as
 * it's stored internally. query is the full name as would be given to
 * 'git config'.
 */
static int cvar_match_section(const char *section, const char *query)
{
	const char *sdot, *qdot, *qsub;
	size_t section_len;

	sdot = strchr(section, '.');

	/* If the section doesn't have any dots, it's easy */
	if (sdot == NULL)
		return !strncasecmp(section, query, strlen(section));

	/*
	 * If it does have dots, compare the sections
	 * case-insensitively. The comparison includes the dots.
	 */
	section_len = sdot - section + 1;
	if (strncasecmp(section, query, sdot - section))
		return 0;

	qsub = query + section_len;
	qdot = strchr(qsub, '.');
	/* Make sure the subsections are the same length */
	if (strlen(sdot + 1) != (size_t) (qdot - qsub))
		return 0;

	/* The subsection is case-sensitive */
	return !strncmp(sdot + 1, qsub, strlen(sdot + 1));
}

static int cvar_match_name(const cvar_t *var, const char *str)
{
	const char *name_start;

	if (!cvar_match_section(var->section, str)) {
		return 0;
	}
	/* Early exit if the lengths are different */
	name_start = strrchr(str, '.') + 1;
	if (strlen(var->name) != strlen(name_start))
		return 0;

	return !strcasecmp(var->name, name_start);
}

static cvar_t *cvar_list_find(cvar_t_list *list, const char *name)
{
	cvar_t *iter;

	CVAR_LIST_FOREACH (list, iter) {
		if (cvar_match_name(iter, name))
			return iter;
	}

	return NULL;
}

static int cvar_normalize_name(cvar_t *var, char **output)
{
	char *section_sp = strchr(var->section, ' ');
	char *quote, *name;
	size_t len;
	int ret;

	/*
	 * The final string is going to be at most one char longer than
	 * the input
	 */
	len = strlen(var->section) + strlen(var->name) + 1;
	name = git__malloc(len + 1);
	if (name == NULL)
		return GIT_ENOMEM;

	/* If there aren't any spaces in the section, it's easy */
	if (section_sp == NULL) {
		ret = p_snprintf(name, len + 1, "%s.%s", var->section, var->name);
		if (ret < 0) {
			git__free(name);
			return git__throw(GIT_EOSERR, "Failed to normalize name. OS err: %s", strerror(errno));
		}

		*output = name;
		return GIT_SUCCESS;
	}

	/*
	 * If there are spaces, we replace the space by a dot, move
	 * section name so it overwrites the first quotation mark and
	 * replace the last quotation mark by a dot. We then append the
	 * variable name.
	 */
	strcpy(name, var->section);
	section_sp = strchr(name, ' ');
	*section_sp = '.';
	/* Remove first quote */
	quote = strchr(name, '"');
	memmove(quote, quote+1, strlen(quote+1));
	/* Remove second quote */
	quote = strchr(name, '"');
	*quote = '.';
	strcpy(quote+1, var->name);

	*output = name;
	return GIT_SUCCESS;
}

static char *interiorize_section(const char *orig)
{
	char *dot, *last_dot, *section, *ret;
	size_t len;

	dot = strchr(orig, '.');
	last_dot = strrchr(orig, '.');
	len = last_dot - orig;

	/* No subsection, this is easy */
	if (last_dot == dot)
		return git__strndup(orig, dot - orig);

	section = git__malloc(len + 4);
	if (section == NULL)
		return NULL;

	memset(section, 0x0, len + 4);
	ret = section;
	len = dot - orig;
	memcpy(section, orig, len);
	section += len;
	len = strlen(" \"");
	memcpy(section, " \"", len);
	section += len;
	len = last_dot - dot - 1;
	memcpy(section, dot + 1, len);
	section += len;
	*section = '"';

	return ret;
}

static int config_open(git_config_file *cfg)
{
	int error;
	diskfile_backend *b = (diskfile_backend *)cfg;

	error = git_futils_readbuffer(&b->reader.buffer, b->file_path);
	if(error < GIT_SUCCESS)
		goto cleanup;

	error = config_parse(b);
	if (error < GIT_SUCCESS)
		goto cleanup;

	git_futils_freebuffer(&b->reader.buffer);

	return error;

 cleanup:
	cvar_list_free(&b->var_list);
	git_futils_freebuffer(&b->reader.buffer);

	return git__rethrow(error, "Failed to open config");
}

static void backend_free(git_config_file *_backend)
{
	diskfile_backend *backend = (diskfile_backend *)_backend;

	if (backend == NULL)
		return;

	git__free(backend->file_path);
	cvar_list_free(&backend->var_list);

	git__free(backend);
}

static int file_foreach(git_config_file *backend, int (*fn)(const char *, const char *, void *), void *data)
{
	int ret = GIT_SUCCESS;
	cvar_t *var;
	diskfile_backend *b = (diskfile_backend *)backend;

	CVAR_LIST_FOREACH(&b->var_list, var) {
		char *normalized = NULL;

		ret = cvar_normalize_name(var, &normalized);
		if (ret < GIT_SUCCESS)
			return ret;

		ret = fn(normalized, var->value, data);
		git__free(normalized);
		if (ret)
			break;
	}

	return ret;
}

static int config_set(git_config_file *cfg, const char *name, const char *value)
{
	cvar_t *var = NULL;
	cvar_t *existing = NULL;
	int error = GIT_SUCCESS;
	const char *last_dot;
	diskfile_backend *b = (diskfile_backend *)cfg;

	/*
	 * If it already exists, we just need to update its value.
	 */
	existing = cvar_list_find(&b->var_list, name);
	if (existing != NULL) {
		char *tmp = value ? git__strdup(value) : NULL;
		if (tmp == NULL && value != NULL)
			return GIT_ENOMEM;

		git__free(existing->value);
		existing->value = tmp;

		return config_write(b, existing);
	}

	/*
	 * Otherwise, create it and stick it at the end of the queue. If
	 * value is NULL, we return an error, because you can't delete a
	 * variable that doesn't exist.
	 */

	if (value == NULL)
		return git__throw(GIT_ENOTFOUND, "Can't delete non-exitent variable");

	last_dot = strrchr(name, '.');
	if (last_dot == NULL) {
		return git__throw(GIT_EINVALIDTYPE, "Variables without section aren't allowed");
	}

	var = git__malloc(sizeof(cvar_t));
	if (var == NULL)
		return GIT_ENOMEM;

	memset(var, 0x0, sizeof(cvar_t));

	var->section = interiorize_section(name);
	if (var->section == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	var->name = git__strdup(last_dot + 1);
	if (var->name == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	var->value = value ? git__strdup(value) : NULL;
	if (var->value == NULL && value != NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	CVAR_LIST_APPEND(&b->var_list, var);
	error = config_write(b, var);

 out:
	if (error < GIT_SUCCESS)
		cvar_free(var);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to set config value");
}

/*
 * Internal function that actually gets the value in string form
 */
static int config_get(git_config_file *cfg, const char *name, const char **out)
{
	cvar_t *var;
	int error = GIT_SUCCESS;
	diskfile_backend *b = (diskfile_backend *)cfg;

	var = cvar_list_find(&b->var_list, name);

	if (var == NULL)
		return git__throw(GIT_ENOTFOUND, "Variable '%s' not found", name);

	*out = var->value;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to get config value for %s", name);
}

int git_config_file__ondisk(git_config_file **out, const char *path)
{
	diskfile_backend *backend;

	backend = git__malloc(sizeof(diskfile_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	memset(backend, 0x0, sizeof(diskfile_backend));

	backend->file_path = git__strdup(path);
	if (backend->file_path == NULL) {
		git__free(backend);
		return GIT_ENOMEM;
	}

	backend->parent.open = config_open;
	backend->parent.get = config_get;
	backend->parent.set = config_set;
	backend->parent.foreach = file_foreach;
	backend->parent.free = backend_free;

	*out = (git_config_file *)backend;

	return GIT_SUCCESS;
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
	while (skip_whitespace && isspace(c));

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
static char *cfg_readline(diskfile_backend *cfg)
{
	char *line = NULL;
	char *line_src, *line_end;
	size_t line_len;

	line_src = cfg->reader.read_ptr;

	/* Skip empty empty lines */
	while (isspace(*line_src))
		++line_src;

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
	while (line_len-- > 0 && isspace(line[line_len]));

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

static int parse_section_header_ext(const char *line, const char *base_name, char **section_name)
{
	int c, rpos;
	char *first_quote, *last_quote;
	git_buf buf = GIT_BUF_INIT;
	int error = GIT_SUCCESS;
	int quote_marks;
	/*
	 * base_name is what came before the space. We should be at the
	 * first quotation mark, except for now, line isn't being kept in
	 * sync so we only really use it to calculate the length.
	 */

	first_quote = strchr(line, '"');
	last_quote = strrchr(line, '"');

	if (last_quote - first_quote == 0)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse ext header. There is no final quotation mark");

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
			error = git__throw(GIT_EOBJCORRUPTED, "Falied to parse ext header. Text after closing quote");
			goto out;

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
				error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse ext header. Unsupported escape char \\%c", c);
				goto out;
			}
			break;
		default:
			break;
		}

		git_buf_putc(&buf, c);
	} while ((c = line[rpos++]) != ']');

	*section_name = git__strdup(git_buf_cstr(&buf));
 out:
	git_buf_free(&buf);

	return error;
}

static int parse_section_header(diskfile_backend *cfg, char **section_out)
{
	char *name, *name_end;
	int name_length, c, pos;
	int error = GIT_SUCCESS;
	char *line;

	line = cfg_readline(cfg);
	if (line == NULL)
		return GIT_ENOMEM;

	/* find the end of the variable's name */
	name_end = strchr(line, ']');
	if (name_end == NULL) {
		git__free(line);
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Can't find header name end");
	}

	name = (char *)git__malloc((size_t)(name_end - line) + 1);
	if (name == NULL) {
		git__free(line);
		return GIT_ENOMEM;
	}

	name_length = 0;
	pos = 0;

	/* Make sure we were given a section header */
	c = line[pos++];
	if (c != '[') {
		error = git__throw(GIT_ERROR, "Failed to parse header. Didn't get section header. This is a bug");
		goto error;
	}

	c = line[pos++];

	do {
		if (isspace(c)){
			name[name_length] = '\0';
			error = parse_section_header_ext(line, name, section_out);
			git__free(line);
			git__free(name);
			return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse header");
		}

		if (!config_keychar(c) && c != '.') {
			error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Wrong format on header");
			goto error;
		}

		name[name_length++] = (char) tolower(c);

	} while ((c = line[pos++]) != ']');

	if (line[pos - 1] != ']') {
		error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Config file ended unexpectedly");
		goto error;
	}

	name[name_length] = 0;
	git__free(line);
	git__strtolower(name);
	*section_out = name;
	return GIT_SUCCESS;

error:
	git__free(line);
	git__free(name);
	return error;
}

static int skip_bom(diskfile_backend *cfg)
{
	static const char *utf8_bom = "\xef\xbb\xbf";

	if (memcmp(cfg->reader.read_ptr, utf8_bom, sizeof(utf8_bom)) == 0)
		cfg->reader.read_ptr += sizeof(utf8_bom);

	/* TODO: the reference implementation does pretty stupid
		shit with the BoM
	*/

	return GIT_SUCCESS;
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

static void strip_comments(char *line)
{
	int quote_count = 0;
	char *ptr;

	for (ptr = line; *ptr; ++ptr) {
		if (ptr[0] == '"' && ptr > line && ptr[-1] != '\\')
			quote_count++;

		if ((ptr[0] == ';' || ptr[0] == '#') && (quote_count % 2) == 0) {
			ptr[0] = '\0';
			break;
		}
	}

	if (isspace(ptr[-1])) {
		/* TODO skip whitespace */
	}
}

static int config_parse(diskfile_backend *cfg_file)
{
	int error = GIT_SUCCESS, c;
	char *current_section = NULL;
	char *var_name;
	char *var_value;
	cvar_t *var;

	/* Initialize the reading position */
	cfg_file->reader.read_ptr = cfg_file->reader.buffer.data;
	cfg_file->reader.eof = 0;

	/* If the file is empty, there's nothing for us to do */
	if (*cfg_file->reader.read_ptr == '\0')
		return GIT_SUCCESS;

	skip_bom(cfg_file);

	while (error == GIT_SUCCESS && !cfg_file->reader.eof) {

		c = cfg_peek(cfg_file, SKIP_WHITESPACE);

		switch (c) {
		case '\0': /* We've arrived at the end of the file */
			break;

		case '[': /* section header, new section begins */
			git__free(current_section);
			current_section = NULL;
			error = parse_section_header(cfg_file, &current_section);
			break;

		case ';':
		case '#':
			cfg_consume_line(cfg_file);
			break;

		default: /* assume variable declaration */
			error = parse_variable(cfg_file, &var_name, &var_value);

			if (error < GIT_SUCCESS)
				break;

			var = git__malloc(sizeof(cvar_t));
			if (var == NULL) {
				error = GIT_ENOMEM;
				break;
			}

			memset(var, 0x0, sizeof(cvar_t));

			var->section = git__strdup(current_section);
			if (var->section == NULL) {
				error = GIT_ENOMEM;
				git__free(var);
				break;
			}

			var->name = var_name;
			var->value = var_value;
			git__strtolower(var->name);

			CVAR_LIST_APPEND(&cfg_file->var_list, var);

			break;
		}
	}

	git__free(current_section);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse config");
}

static int write_section(git_filebuf *file, cvar_t *var)
{
	int error;

	error = git_filebuf_printf(file, "[%s]\n", var->section);
	if (error < GIT_SUCCESS)
		return error;

	error = git_filebuf_printf(file, "    %s = %s\n", var->name, var->value);
	return error;
}

/*
 * This is pretty much the parsing, except we write out anything we don't have
 */
static int config_write(diskfile_backend *cfg, cvar_t *var)
{
	int error = GIT_SUCCESS, c;
	int section_matches = 0, last_section_matched = 0;
	char *current_section = NULL;
	char *var_name, *var_value, *data_start;
	git_filebuf file;
	const char *pre_end = NULL, *post_start = NULL;

	/* We need to read in our own config file */
	error = git_futils_readbuffer(&cfg->reader.buffer, cfg->file_path);
	if (error < GIT_SUCCESS) {
		return git__rethrow(error, "Failed to read existing config file %s", cfg->file_path);
	}

	/* Initialise the reading position */
	cfg->reader.read_ptr = cfg->reader.buffer.data;
	cfg->reader.eof = 0;
	data_start = cfg->reader.read_ptr;

	/* Lock the file */
	error = git_filebuf_open(&file, cfg->file_path, 0);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lock config file");

	skip_bom(cfg);

	while (error == GIT_SUCCESS && !cfg->reader.eof) {
		c = cfg_peek(cfg, SKIP_WHITESPACE);

		switch (c) {
		case '\0': /* We've arrived at the end of the file */
			break;

		case '[': /* section header, new section begins */
			/*
			 * We set both positions to the current one in case we
			 * need to add a variable to the end of a section. In that
			 * case, we want both variables to point just before the
			 * new section. If we actually want to replace it, the
			 * default case will take care of updating them.
			 */
			pre_end = post_start = cfg->reader.read_ptr;
			if (current_section)
				git__free(current_section);
			error = parse_section_header(cfg, &current_section);
			if (error < GIT_SUCCESS)
				break;

			/* Keep track of when it stops matching */
			last_section_matched = section_matches;
			section_matches = !strcmp(current_section, var->section);
			break;

		case ';':
		case '#':
			cfg_consume_line(cfg);
			break;

		default:
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
					break;
				}
			} else {
				int cmp = -1;

				pre_end = cfg->reader.read_ptr;
				if ((error = parse_variable(cfg, &var_name, &var_value)) == GIT_SUCCESS)
					cmp = strcasecmp(var->name, var_name);

				git__free(var_name);
				git__free(var_value);

				if (cmp != 0)
					break;

				post_start = cfg->reader.read_ptr;
			}

			/*
			 * We've found the variable we wanted to change, so
			 * write anything up to it
			 */
			error = git_filebuf_write(&file, data_start, pre_end - data_start);
			if (error < GIT_SUCCESS) {
				git__rethrow(error, "Failed to write the first part of the file");
				break;
			}

			/*
			 * Then replace the variable. If the value is NULL, it
			 * means we want to delete it, so pretend everything went
			 * fine
			 */
			if (var->value == NULL)
				error = GIT_SUCCESS;
			else
				error = git_filebuf_printf(&file, "\t%s = %s\n", var->name, var->value);
			if (error < GIT_SUCCESS) {
				git__rethrow(error, "Failed to overwrite the variable");
				break;
			}

			/* And then the write out rest of the file */
			error = git_filebuf_write(&file, post_start,
						cfg->reader.buffer.len - (post_start - data_start));

			if (error < GIT_SUCCESS) {
				git__rethrow(error, "Failed to write the rest of the file");
					break;
			}

			goto cleanup;
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
	 * Either way we need to write out the whole file.
	 */

	error = git_filebuf_write(&file, cfg->reader.buffer.data, cfg->reader.buffer.len);
	if (error < GIT_SUCCESS) {
		git__rethrow(error, "Failed to write original config content");
		goto cleanup;
	}

	/* And now if we just need to add a variable */
	if (section_matches) {
		error = git_filebuf_printf(&file, "\t%s = %s\n", var->name, var->value);
		goto cleanup;
	}

	/* Or maybe we need to write out a whole section */
	error = write_section(&file, var);
	if (error < GIT_SUCCESS)
		git__rethrow(error, "Failed to write new section");

 cleanup:
	git__free(current_section);

	if (error < GIT_SUCCESS)
		git_filebuf_cleanup(&file);
	else
		error = git_filebuf_commit(&file, GIT_CONFIG_FILE_MODE);

	git_futils_freebuffer(&cfg->reader.buffer);
	return error;
}

static int is_multiline_var(const char *str)
{
	char *end = strrchr(str, '\0') - 1;

	while (isspace(*end))
		--end;

	return *end == '\\';
}

static int parse_multiline_variable(diskfile_backend *cfg, const char *first, char **out)
{
	char *line = NULL, *end;
	int error = GIT_SUCCESS, ret;
	size_t len;
	char *buf;

	/* Check that the next line exists */
	line = cfg_readline(cfg);
	if (line == NULL)
		return GIT_ENOMEM;

	/* We've reached the end of the file, there is input missing */
	if (line[0] == '\0') {
		error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse multiline var. File ended unexpectedly");
		goto out;
	}

	strip_comments(line);

	/* If it was just a comment, pretend it didn't exist */
	if (line[0] == '\0') {
		error = parse_multiline_variable(cfg, first, out);
		goto out;
	}

	/* Find the continuation character '\' and strip the whitespace */
	end = strrchr(first, '\\');
	while (isspace(end[-1]))
		--end;

	*end = '\0'; /* Terminate the string here */

	len = strlen(first) + strlen(line) + 2;
	buf = git__malloc(len);
	if (buf == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	ret = p_snprintf(buf, len, "%s %s", first, line);
	if (ret < 0) {
		error = git__throw(GIT_EOSERR, "Failed to parse multiline var. Failed to put together two lines. OS err: %s", strerror(errno));
		git__free(buf);
		goto out;
	}

	/*
	 * If we need to continue reading the next line, pretend
	 * everything we've read up to now was in one line and call
	 * ourselves.
	 */
	if (is_multiline_var(buf)) {
		char *final_val;
		error = parse_multiline_variable(cfg, buf, &final_val);
		git__free(buf);
		buf = final_val;
	}

	*out = buf;

 out:
	git__free(line);
	return error;
}

static int parse_variable(diskfile_backend *cfg, char **var_name, char **var_value)
{
	char *tmp;
	int error = GIT_SUCCESS;
	const char *var_end = NULL;
	const char *value_start = NULL;
	char *line;

	line = cfg_readline(cfg);
	if (line == NULL)
		return GIT_ENOMEM;

	strip_comments(line);

	var_end = strchr(line, '=');

	if (var_end == NULL)
		var_end = strchr(line, '\0');
	else
		value_start = var_end + 1;

	if (isspace(var_end[-1])) {
		do var_end--;
		while (isspace(var_end[0]));
	}

	tmp = git__strndup(line, var_end - line + 1);
	if (tmp == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	*var_name = tmp;

	/*
	 * Now, let's try to parse the value
	 */
	if (value_start != NULL) {

		while (isspace(value_start[0]))
			value_start++;

		if (value_start[0] == '\0') {
			*var_value = NULL;
			goto out;
		}

		if (is_multiline_var(value_start)) {
			error = parse_multiline_variable(cfg, value_start, var_value);
			if (error != GIT_SUCCESS)
			{
				*var_value = NULL;
				git__free(*var_name);
			}
			goto out;
		}

		tmp = git__strdup(value_start);
		if (tmp == NULL) {
			git__free(*var_name);
			*var_value = NULL;
			error = GIT_ENOMEM;
			goto out;
		}

		*var_value = tmp;
	} else {
		/* If there is no value, boolean true is assumed */
		*var_value = NULL;
	}

 out:
	git__free(line);
	return error;
}
