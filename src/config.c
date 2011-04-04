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
#include "fileops.h"
#include "hashtable.h"
#include "config.h"

#include <ctype.h>

/**********************
 * Forward declarations
 ***********************/
static int config_parse(git_config *cfg_file);
static int parse_variable(git_config *cfg, char **var_name, char **var_value);
void git_config_free(git_config *cfg);

static git_cvar *cvar_free(git_cvar *var)
{
	git_cvar *next = var->next;

	free(var->name);
	free(var->value);
	free(var);

	return next;
}

static void cvar_list_free(git_cvar *start)
{
	git_cvar *iter = start;

	while ((iter = cvar_free(iter)) != NULL);
}

/*
 * The order is important. The first parameter is the name we want to
 * match against, and the second one is what we're looking for
 */
static int cvar_section_match(const char *local, const char *input)
{
	char *input_dot = strrchr(input, '.');
	char *local_last_dot = strrchr(local, '.');
	char *local_sp = strchr(local, ' ');
	int comparison_len;

	/*
	 * If the local section name doesn't contain a space, then we can
	 * just do a case-insensitive compare.
	 */
	if (local_sp == NULL)
		return !strncasecmp(local, input, local_last_dot - local);

	/* Anything before the space in local is case-insensitive */
	if (strncasecmp(local, input, local_sp - local))
		return 0;

	/*
	 * We compare starting from the first character after the
	 * quotation marks, which is two characters beyond the space. For
	 * the input, we start one character beyond the first dot.
	 * The length is given by the length between the quotation marks.
	 *
	 * this "that".var
	 *       ^    ^
	 *       a    b
	 *
	 * where a is (local_sp + 2) and b is local_last_dot. The comparison
	 * length is given by b - 1 - a.
	 */
	input_dot = strchr(input, '.');
	comparison_len = local_last_dot - 1 - (local_sp + 2);
	return !strncmp(local_sp + 2, input_dot + 1, comparison_len);
}

static int cvar_name_match(const char *local, const char *input)
{
	char *input_dot = strrchr(input, '.');
	char *local_dot = strrchr(local, '.');

	/*
	 * First try to match the section name
	 */
	if (!cvar_section_match(local, input))
		return 0;

	/*
	 * Anything after the last (possibly only) dot is case-insensitive
	 */
	if (!strcmp(input_dot, local_dot))
		return 1;

	return 0;
}

static git_cvar *cvar_list_find(git_cvar *start, const char *name)
{
	git_cvar *iter;

	CVAR_LIST_FOREACH (start, iter) {
		if (cvar_name_match(iter->name, name))
			return iter;
	}

	return NULL;
}

static int cvar_name_normalize(const char *input, char **output)
{
	char *input_sp = strchr(input, ' ');
	char *quote, *str;
	int i;

	/* We need to make a copy anyway */
	str = git__strdup(input);
	if (str == NULL)
		return GIT_ENOMEM;

	*output = str;

	/* If there aren't any spaces, we don't need to do anything */
	if (input_sp == NULL)
		return GIT_SUCCESS;

	/*
	 * If there are spaces, we replace the space by a dot, move the
	 * variable name so that the dot before it replaces the last
	 * quotation mark and repeat so that the first quotation mark
	 * disappears.
	 */
	str[input_sp - input] = '.';

	for (i = 0; i < 2; ++i) {
		quote = strrchr(str, '"');
		memmove(quote, quote + 1, strlen(quote));
	}

	return GIT_SUCCESS;
}

void strntolower(char *str, int len)
{
	int i;

	for (i = 0; i < len; ++i) {
		str[len] = tolower(str[len]);
	}
}

void strtolower(char *str)
{
	strntolower(str, strlen(str));
}

int git_config_open(git_config **cfg_out, const char *path)
{
	git_config *cfg;
	int error = GIT_SUCCESS;

	assert(cfg_out && path);

	cfg = git__malloc(sizeof(git_config));
	if (cfg == NULL)
		return GIT_ENOMEM;

	memset(cfg, 0x0, sizeof(git_config));

	cfg->file_path = git__strdup(path);
	if (cfg->file_path == NULL){
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = gitfo_read_file(&cfg->reader.buffer, cfg->file_path);
	if(error < GIT_SUCCESS)
		goto cleanup;

	error = config_parse(cfg);
	if(error < GIT_SUCCESS)
		goto cleanup;
	else
		*cfg_out = cfg;

	return error;

 cleanup:
	if(cfg->vars)
		cvar_list_free(cfg->vars);
	if(cfg->file_path)
		free(cfg->file_path);
	gitfo_free_buf(&cfg->reader.buffer);
	free(cfg);

	return error;
}

void git_config_free(git_config *cfg)
{
	if (cfg == NULL)
		return;

	free(cfg->file_path);
	cvar_list_free(cfg->vars);
	gitfo_free_buf(&cfg->reader.buffer);

	free(cfg);
}

/*
 * Loop over all the variables
 */

int git_config_foreach(git_config *cfg, int (*fn)(const char *, void *), void *data)
{
	int ret = GIT_SUCCESS;
	git_cvar *var;
	char *normalized;

	CVAR_LIST_FOREACH(cfg->vars, var) {
		ret = cvar_name_normalize(var->name, &normalized);
		if (ret < GIT_SUCCESS)
			return ret;

		ret = fn(normalized, data);
		free(normalized);
		if (ret)
			break;
	}

	return ret;
}

/**************
 * Setters
 **************/

/*
 * Internal function to actually set the string value of a variable
 */
static int config_set(git_config *cfg, const char *name, const char *value)
{
	git_cvar *var = NULL;
	git_cvar *existing = NULL;
	int error = GIT_SUCCESS;

	/*
	 * If it already exists, we just need to update its value.
	 */
	existing = cvar_list_find(cfg->vars, name);
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

	var = git__malloc(sizeof(git_cvar));
	if(var == NULL){
		error = GIT_ENOMEM;
		goto out;
	}

	var->name = git__strdup(name);
	if(var->name == NULL){
		error = GIT_ENOMEM;
		free(var);
		goto out;
	}

	var->value = value ? git__strdup(value) : NULL;
	if(var->value == NULL && value != NULL){
		error = GIT_ENOMEM;
		cvar_free(var);
		goto out;
	}

	var->next = NULL;

	if (cfg->vars_tail == NULL) {
		cfg->vars = cfg->vars_tail = var;
	}
	else {
		cfg->vars_tail->next = var;
		cfg->vars_tail = var;
	}

 out:
	if(error < GIT_SUCCESS)
		cvar_free(var);

	return error;

}

int git_config_set_int(git_config *cfg, const char *name, int value)
{
	char str_value[5]; /* Most numbers should fit in here */
	int buf_len = sizeof(str_value), ret;
	char *help_buf = NULL;

	if((ret = snprintf(str_value, buf_len, "%d", value)) >= buf_len - 1){
		/* The number is too large, we need to allocate more memory */
		buf_len = ret + 1;
		help_buf = git__malloc(buf_len);
		snprintf(help_buf, buf_len, "%d", value);
		ret = config_set(cfg, name, help_buf);
		free(help_buf);
	} else {
		ret = config_set(cfg, name, str_value);
	}

	return ret;
}

int git_config_set_bool(git_config *cfg, const char *name, int value)
{
	const char *str_value;

	if(value == 0)
		str_value = "false";
	else
		str_value = "true";

	return config_set(cfg, name, str_value);
}

int git_config_set_string(git_config *cfg, const char *name, const char *value)
{
	return config_set(cfg, name, value);
}

/***********
 * Getters
 ***********/

/*
 * Internal function that actually gets the value in string form
 */
static int config_get(git_config *cfg, const char *name, const char **out)
{
	git_cvar *var;
	int error = GIT_SUCCESS;

	var = cvar_list_find(cfg->vars, name);

	if (var == NULL)
		return GIT_ENOTFOUND;

	*out = var->value;

	return error;
}

int git_config_get_int(git_config *cfg, const char *name, int *out)
{
	const char *value;
	int ret;

	ret = config_get(cfg, name, &value);
	if(ret < GIT_SUCCESS)
		return ret;

	ret = sscanf(value, "%d", out);
	if (ret == 0) /* No items were matched i.e. value isn't a number */
		return GIT_EINVALIDTYPE;
	if (ret < 0) {
		if (errno == EINVAL) /* Format was NULL */
			return GIT_EINVALIDTYPE;
		else
			return GIT_EOSERR;
	}

	return GIT_SUCCESS;
}

int git_config_get_bool(git_config *cfg, const char *name, int *out)
{
	const char *value;
	int error = GIT_SUCCESS;

	error = config_get(cfg, name, &value);
	if (error < GIT_SUCCESS)
		return error;

	/* A missing value means true */
	if (value == NULL) {
		*out = 1;
		return GIT_SUCCESS;
	}

	if (!strcasecmp(value, "true") ||
		!strcasecmp(value, "yes") ||
		!strcasecmp(value, "on")){
		*out = 1;
		return GIT_SUCCESS;
	}
	if (!strcasecmp(value, "false") ||
		!strcasecmp(value, "no") ||
		!strcasecmp(value, "off")){
		*out = 0;
		return GIT_SUCCESS;
	}

	/* Try to parse it as an integer */
	error = git_config_get_int(cfg, name, out);
	if (error == GIT_SUCCESS)
		*out = !!(*out);

	return error;
}

int git_config_get_string(git_config *cfg, const char *name, const char **out)
{
	return config_get(cfg, name, out);
}

static int cfg_getchar_raw(git_config *cfg)
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

static int cfg_getchar(git_config *cfg_file, int flags)
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
static int cfg_peek(git_config *cfg, int flags)
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
static char *cfg_readline(git_config *cfg)
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
void cfg_consume_line(git_config *cfg)
{
	char *line_start, *line_end;
	int len;

	line_start = cfg->reader.read_ptr;
	line_end = strchr(line_start, '\n');
	/* No newline at EOF */
	if(line_end == NULL){
		line_end = strchr(line_start, '\0');
	}

	len = line_end - line_start;

	if (*line_end == '\n')
		line_end++;

	if (*line_end == '\0')
		cfg->reader.eof = 1;

	cfg->reader.line_number++;
	cfg->reader.read_ptr = line_end;
}

static inline int config_keychar(int c)
{
	return isalnum(c) || c == '-';
}

/*
 * Returns $section.$name, using only name_len chars from the name,
 * which is useful so we don't have to copy the variable name
 * twice. The name of the variable is set to lowercase.
 * Don't forget to free the buffer.
 */
static char *build_varname(const char *section, const char *name)
{
	char *varname;
	int section_len, ret;
	int name_len;
	size_t total_len;

	name_len = strlen(name);
	section_len = strlen(section);
	total_len = section_len + name_len + 2;
	varname = malloc(total_len);
	if(varname == NULL)
		return NULL;

	ret = snprintf(varname, total_len, "%s.%s", section, name);
	if(ret >= 0){ /* lowercase from the last dot onwards */
		char *dot = strrchr(varname, '.');
		if (dot != NULL)
			strtolower(dot);
	}

	return varname;
}

static int parse_section_header_ext(const char *line, const char *base_name, char **section_name)
{
	int buf_len, total_len, pos, rpos;
	int c;
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
		return GIT_EOBJCORRUPTED;

	buf_len = last_quote - first_quote + 2;

	subsection = git__malloc(buf_len + 2);
	if(subsection == NULL)
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
		switch(c) {
		case '"':
			if (quote_marks++ >= 2)
				return GIT_EOBJCORRUPTED;
			break;
		case '\\':
			c = line[rpos++];
			switch (c) {
			case '"':
			case '\\':
				break;
			default:
				error = GIT_EOBJCORRUPTED;
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

	sprintf(*section_name, "%s %s", base_name, subsection);
	strntolower(*section_name, strchr(*section_name, ' ') - *section_name);

 out:
	free(subsection);

	return error;
}

static int parse_section_header(git_config *cfg, char **section_out)
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
		return GIT_EOBJCORRUPTED;

	name = (char *)git__malloc((size_t)(name_end - line) + 1);
	if (name == NULL)
		return GIT_EOBJCORRUPTED;

	name_length = 0;
	pos = 0;

	/* Make sure we were given a section header */
	c = line[pos++];
	if(c != '['){
		error = GIT_EOBJCORRUPTED;
		goto error;
	}

	c = line[pos++];

	do {
		if (cfg->reader.eof){
			error = GIT_EOBJCORRUPTED;
			goto error;
		}

		if (isspace(c)){
			name[name_length] = '\0';
			error = parse_section_header_ext(line, name, section_out);
			free(line);
			free(name);
			return error;
		}

		if (!config_keychar(c) && c != '.'){
			error = GIT_EOBJCORRUPTED;
			goto error;
		}

		name[name_length++] = tolower(c);

	} while ((c = line[pos++]) != ']');

	name[name_length] = 0;
	free(line);
	strtolower(name);
	*section_out = name;
	return GIT_SUCCESS;

error:
	free(line);
	free(name);
	return error;
}

static int skip_bom(git_config *cfg)
{
	static const unsigned char *utf8_bom = "\xef\xbb\xbf";

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

static int config_parse(git_config *cfg_file)
{
	int error = GIT_SUCCESS, c;
	char *current_section = NULL;
	char *var_name;
	char *var_value;
	char *full_name;

	/* Initialise the reading position */
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

			full_name = build_varname(current_section, var_name);
			if (full_name == NULL) {
				error = GIT_ENOMEM;
				free(var_name);
				free(var_value);
				break;
			}

			config_set(cfg_file, full_name, var_value);
			free(var_name);
			free(var_value);
			free(full_name);

			break;
		}
	}

	if(current_section)
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

static int parse_multiline_variable(git_config *cfg, const char *first, char **out)
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
		error = GIT_EOBJCORRUPTED;
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
		error = GIT_EOSERR;
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

static int parse_variable(git_config *cfg, char **var_name, char **var_value)
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

	tmp = strndup(line, var_end - line + 1);
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
		/* If thre is no value, boolean true is assumed */
		*var_value = NULL;
	}

 out:
	free(line);
	return error;
}
