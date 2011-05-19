/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "config.h"
#include "fileops.h"
#include "git2/config_backend.h"
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
	git_config_backend parent;

	cvar_t_list var_list;

	struct {
		gitfo_buf buffer;
		char *read_ptr;
		int line_number;
		int eof;
	} reader;

	char *file_path;
} file_backend;

static int config_parse(file_backend *cfg_file);
static int parse_variable(file_backend *cfg, char **var_name, char **var_value);

static void cvar_free(cvar_t *var)
{
	if (var == NULL)
		return;

	free(var->section);
	free(var->name);
	free(var->value);
	free(var);
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
 * Compare two strings according to the git section-subsection
 * rules. The order of the strings is important because local is
 * assumed to have the internal format (only the section name and with
 * case information) and input the normalized one (only dots, no case
 * information).
 */
static int cvar_match_section(const char *local, const char *input)
{
	char *first_dot, *last_dot;
	char *local_sp = strchr(local, ' ');
	int comparison_len;

	/*
	 * If the local section name doesn't contain a space, then we can
	 * just do a case-insensitive compare.
	 */
	if (local_sp == NULL)
		return !strncasecmp(local, input, strlen(local));

	/*
	 * From here onwards, there is a space diving the section and the
	 * subsection. Anything before the space in local is
	 * case-insensitive.
	 */
	if (strncasecmp(local, input, local_sp - local))
		return 0;

	/*
	 * We compare starting from the first character after the
	 * quotation marks, which is two characters beyond the space. For
	 * the input, we start one character beyond the dot. If the names
	 * have different lengths, then we can fail early, as we know they
	 * can't be the same.
	 * The length is given by the length between the quotation marks.
	 */

	first_dot = strchr(input, '.');
	last_dot = strrchr(input, '.');
	comparison_len = strlen(local_sp + 2) - 1;

	if (last_dot == first_dot || last_dot - first_dot - 1 != comparison_len)
		return 0;

	return !strncmp(local_sp + 2, first_dot + 1, comparison_len);
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
	int len, ret;

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
		ret = snprintf(name, len + 1, "%s.%s", var->section, var->name);
		if (ret < 0)
			return git__throw(GIT_EOSERR, "Failed to normalize name. OS err: %s", strerror(errno));

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

static int config_open(git_config_backend *cfg)
{
	int error;
	file_backend *b = (file_backend *)cfg;

	error = gitfo_read_file(&b->reader.buffer, b->file_path);
	if(error < GIT_SUCCESS)
		goto cleanup;

	error = config_parse(b);
	if (error < GIT_SUCCESS)
		goto cleanup;

	gitfo_free_buf(&b->reader.buffer);

	return error;

 cleanup:
	cvar_list_free(&b->var_list);
	gitfo_free_buf(&b->reader.buffer);
	free(cfg);

	return error;
}

static void backend_free(git_config_backend *_backend)
{
	file_backend *backend = (file_backend *)_backend;

	if (backend == NULL)
		return;

	free(backend->file_path);
	cvar_list_free(&backend->var_list);

	free(backend);
}

static int file_foreach(git_config_backend *backend, int (*fn)(const char *, void *), void *data)
{
	int ret = GIT_SUCCESS;
	cvar_t *var;
	char *normalized;
	file_backend *b = (file_backend *)backend;

	CVAR_LIST_FOREACH(&b->var_list, var) {
		ret = cvar_normalize_name(var, &normalized);
		if (ret < GIT_SUCCESS)
			return ret;

		ret = fn(normalized, data);
		free(normalized);
		if (ret)
			break;
	}

	return ret;
}

static int config_set(git_config_backend *cfg, const char *name, const char *value)
{
	cvar_t *var = NULL;
	cvar_t *existing = NULL;
	int error = GIT_SUCCESS;
	const char *last_dot;
	file_backend *b = (file_backend *)cfg;

	/*
	 * If it already exists, we just need to update its value.
	 */
	existing = cvar_list_find(&b->var_list, name);
	if (existing != NULL) {
		char *tmp = value ? git__strdup(value) : NULL;
		if (tmp == NULL && value != NULL)
			return GIT_ENOMEM;

		free(existing->value);
		existing->value = tmp;

		return GIT_SUCCESS;
	}

	/*
	 * Otherwise, create it and stick it at the end of the queue.
	 */

	last_dot = strrchr(name, '.');
	if (last_dot == NULL) {
		return git__throw(GIT_EINVALIDTYPE, "Variables without section aren't allowed");
	}

	var = git__malloc(sizeof(cvar_t));
	if (var == NULL)
		return GIT_ENOMEM;

	memset(var, 0x0, sizeof(cvar_t));

	var->section = git__strndup(name, last_dot - name);
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

 out:
	if (error < GIT_SUCCESS)
		cvar_free(var);

	return error;
}

/*
 * Internal function that actually gets the value in string form
 */
static int config_get(git_config_backend *cfg, const char *name, const char **out)
{
	cvar_t *var;
	int error = GIT_SUCCESS;
	file_backend *b = (file_backend *)cfg;

	var = cvar_list_find(&b->var_list, name);

	if (var == NULL)
		return git__throw(GIT_ENOTFOUND, "Variable '%s' not found", name);

	*out = var->value;

	return error;
}

int git_config_backend_file(git_config_backend **out, const char *path)
{
	file_backend *backend;

	backend = git__malloc(sizeof(file_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	memset(backend, 0x0, sizeof(file_backend));

	backend->file_path = git__strdup(path);
	if (backend->file_path == NULL) {
		free(backend);
		return GIT_ENOMEM;
	}

	backend->parent.open = config_open;
	backend->parent.get = config_get;
	backend->parent.set = config_set;
	backend->parent.foreach = file_foreach;
	backend->parent.free = backend_free;

	*out = (git_config_backend *)backend;

	return GIT_SUCCESS;
}

static int cfg_getchar_raw(file_backend *cfg)
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

static int cfg_getchar(file_backend *cfg_file, int flags)
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
static int cfg_peek(file_backend *cfg, int flags)
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

static const char *LINEBREAK_UNIX = "\\\n";
static const char *LINEBREAK_WIN32 = "\\\r\n";

static int is_linebreak(const char *pos)
{
	return	memcmp(pos - 1, LINEBREAK_UNIX, sizeof(LINEBREAK_UNIX)) == 0 ||
			memcmp(pos - 2, LINEBREAK_WIN32, sizeof(LINEBREAK_WIN32)) == 0;
}

/*
 * Read and consume a line, returning it in newly-allocated memory.
 */
static char *cfg_readline(file_backend *cfg)
{
	char *line = NULL;
	char *line_src, *line_end;
	int line_len;

	line_src = cfg->reader.read_ptr;
    line_end = strchr(line_src, '\n');

    /* no newline at EOF */
	if (line_end == NULL)
		line_end = strchr(line_src, 0);
	else
		while (is_linebreak(line_end))
			line_end = strchr(line_end + 1, '\n');


	while (line_src < line_end && isspace(*line_src))
		line_src++;

	line = (char *)git__malloc((size_t)(line_end - line_src) + 1);
	if (line == NULL)
		return NULL;

	line_len = 0;
	while (line_src < line_end) {

		if (memcmp(line_src, LINEBREAK_UNIX, sizeof(LINEBREAK_UNIX)) == 0) {
			line_src += sizeof(LINEBREAK_UNIX);
			continue;
		}

		if (memcmp(line_src, LINEBREAK_WIN32, sizeof(LINEBREAK_WIN32)) == 0) {
			line_src += sizeof(LINEBREAK_WIN32);
			continue;
		}

		line[line_len++] = *line_src++;
	}

	line[line_len] = '\0';

	while (--line_len >= 0 && isspace(line[line_len]))
		line[line_len] = '\0';

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
void cfg_consume_line(file_backend *cfg)
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
	int buf_len, total_len, pos, rpos;
	int c, ret;
	char *subsection, *first_quote, *last_quote;
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

	buf_len = last_quote - first_quote + 2;

	subsection = git__malloc(buf_len + 2);
	if (subsection == NULL)
		return GIT_ENOMEM;

	pos = 0;
	rpos = 0;
	quote_marks = 0;

	line = first_quote;
	c = line[rpos++];

	/*
	 * At the end of each iteration, whatever is stored in c will be
	 * added to the string. In case of error, jump to out
	 */
	do {
		switch (c) {
		case '"':
			if (quote_marks++ >= 2)
				return git__throw(GIT_EOBJCORRUPTED, "Failed to parse ext header. Too many quotes");
			break;
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
		default:
			break;
		}

		subsection[pos++] = c;
	} while ((c = line[rpos++]) != ']');

	subsection[pos] = '\0';

	total_len = strlen(base_name) + strlen(subsection) + 2;
	*section_name = git__malloc(total_len);
	if (*section_name == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	ret = snprintf(*section_name, total_len, "%s %s", base_name, subsection);
	if (ret >= total_len) {
		/* If this fails, we've checked the length wrong */
		error = git__throw(GIT_ERROR, "Failed to parse ext header. Wrong total length calculation");
		goto out;
	} else if (ret < 0) {
		error = git__throw(GIT_EOSERR, "Failed to parse ext header. OS error: %s", strerror(errno));
		goto out;
	}

	git__strntolower(*section_name, strchr(*section_name, ' ') - *section_name);

 out:
	free(subsection);

	return error;
}

static int parse_section_header(file_backend *cfg, char **section_out)
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
	if (name_end == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Can't find header name end");

	name = (char *)git__malloc((size_t)(name_end - line) + 1);
	if (name == NULL)
		return GIT_ENOMEM;

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
		if (cfg->reader.eof){
			error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Config file ended unexpectedly");
			goto error;
		}

		if (isspace(c)){
			name[name_length] = '\0';
			error = parse_section_header_ext(line, name, section_out);
			free(line);
			free(name);
			return error;
		}

		if (!config_keychar(c) && c != '.') {
			error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse header. Wrong format on header");
			goto error;
		}

		name[name_length++] = tolower(c);

	} while ((c = line[pos++]) != ']');

	name[name_length] = 0;
	free(line);
	git__strtolower(name);
	*section_out = name;
	return GIT_SUCCESS;

error:
	free(line);
	free(name);
	return error;
}

static int skip_bom(file_backend *cfg)
{
	static const char *utf8_bom = "\xef\xbb\xbf";

	if (memcmp(cfg->reader.read_ptr, utf8_bom, sizeof(utf8_bom)) == 0)
		cfg->reader.read_ptr += sizeof(utf8_bom);

	/*  TODO: the reference implementation does pretty stupid
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

static int config_parse(file_backend *cfg_file)
{
	int error = GIT_SUCCESS, c;
	char *current_section = NULL;
	char *var_name;
	char *var_value;
	cvar_t *var;

	/* Initialize the reading position */
	cfg_file->reader.read_ptr = cfg_file->reader.buffer.data;
	cfg_file->reader.eof = 0;

	skip_bom(cfg_file);

	while (error == GIT_SUCCESS && !cfg_file->reader.eof) {

		c = cfg_peek(cfg_file, SKIP_WHITESPACE);

		switch (c) {
		case '\0': /* We've arrived at the end of the file */
			break;

		case '[': /* section header, new section begins */
			free(current_section);
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

			var = malloc(sizeof(cvar_t));
			if (var == NULL) {
				error = GIT_ENOMEM;
				break;
			}

			memset(var, 0x0, sizeof(cvar_t));

			var->section = git__strdup(current_section);
			if (var->section == NULL) {
				error = GIT_ENOMEM;
				free(var);
				break;
			}

			var->name = var_name;
			var->value = var_value;
			git__strtolower(var->name);

			CVAR_LIST_APPEND(&cfg_file->var_list, var);

			break;
		}
	}

	if (current_section)
		free(current_section);

	return error;
}

static int is_multiline_var(const char *str)
{
	char *end = strrchr(str, '\0') - 1;

	while (isspace(*end))
		--end;

	return *end == '\\';
}

static int parse_multiline_variable(file_backend *cfg, const char *first, char **out)
{
	char *line = NULL, *end;
	int error = GIT_SUCCESS, len, ret;
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

	ret = snprintf(buf, len, "%s %s", first, line);
	if (ret < 0) {
		error = git__throw(GIT_EOSERR, "Failed to parse multiline var. Failed to put together two lines. OS err: %s", strerror(errno));
		free(buf);
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
		free(buf);
		buf = final_val;
	}

	*out = buf;

 out:
	free(line);
	return error;
}

static int parse_variable(file_backend *cfg, char **var_name, char **var_value)
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

		if (value_start[0] == '\0')
			goto out;

		if (is_multiline_var(value_start)) {
			error = parse_multiline_variable(cfg, value_start, var_value);
			if (error < GIT_SUCCESS)
				free(*var_name);
			goto out;
		}

		tmp = strdup(value_start);
		if (tmp == NULL) {
			free(*var_name);
			error = GIT_ENOMEM;
			goto out;
		}

		*var_value = tmp;
	} else {
		/* If there is no value, boolean true is assumed */
		*var_value = NULL;
	}

 out:
	free(line);
	return error;
}
