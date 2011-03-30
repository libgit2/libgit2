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
static int parse_variable(git_config *cfg, const char *section_name, const char *line);
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
 * FIXME: Only the section name is case-insensitive
 */
static git_cvar *cvar_list_find(git_cvar *start, const char *name)
{
	git_cvar *iter;

	CVAR_LIST_FOREACH (start, iter) {
		if (!strcasecmp(name, iter->name))
			return iter;
	}

	return NULL;
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

	CVAR_LIST_FOREACH(cfg->vars, var) {
		ret = fn(var->name, data);
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
		char *tmp = git__strdup(value);
		if (tmp == NULL)
			return GIT_ENOMEM;

		free(var->value);
		var->value = tmp;

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

	var->value = git__strdup(value);
	if(var->value == NULL){
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
	}

	ret = config_set(cfg, name, str_value);

	if (help_buf != NULL)
		free(help_buf);

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

static const char *LINEBREAK_UNIX = "\\\n";
static const char *LINEBREAK_WIN32 = "\\\r\n";

static int is_linebreak(const char *pos)
{
	return	memcmp(pos - 1, LINEBREAK_UNIX, sizeof(LINEBREAK_UNIX)) == 0 ||
			memcmp(pos - 2, LINEBREAK_WIN32, sizeof(LINEBREAK_WIN32)) == 0;
}

/*
 * Read a line, but don't consume it
 */
static char *cfg_readline(git_config *cfg)
{
	char *line = NULL;
	char *line_src, *line_end;
	int line_len;

	line_src = cfg->reader.read_ptr;
    line_end = strchr(line_src, '\n');

	while (is_linebreak(line_end))
		line_end = strchr(line_end + 1, '\n');

    /* no newline at EOF */
	if (line_end == NULL)
		line_end = strchr(line_src, 0);

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

	/*
	cfg->reader.line_number++;
	cfg->reader.read_ptr = line_end;
	*/

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

static char *parse_section_header_ext(char *base_name, git_config *cfg)
{
	return base_name;
}

static int parse_section_header(git_config *cfg, char **section_out, const char *line)
{
	char *name, *name_end;
	int name_length, c;
	int error = GIT_SUCCESS;

	/* find the end of the variable's name */
	name_end = strchr(line, ']');
	if (name_end == NULL)
		return GIT_EOBJCORRUPTED;

	name = (char *)git__malloc((size_t)(name_end - line) + 1);
	if (name == NULL)
		return GIT_EOBJCORRUPTED;

	name_length = 0;

	/* Make sure we were given a section header */
	c = cfg_getchar(cfg, SKIP_WHITESPACE | SKIP_COMMENTS);
	if(c != '['){
		error = GIT_EOBJCORRUPTED;
		goto error;
	}

	c = cfg_getchar(cfg, SKIP_WHITESPACE | SKIP_COMMENTS);

	do {
		if (cfg->reader.eof){
			error = GIT_EOBJCORRUPTED;
			goto error;
		}

		if (isspace(c)){
			*section_out = parse_section_header_ext(name, cfg);
			return GIT_SUCCESS;
		}

		if (!config_keychar(c) && c != '.'){
			error = GIT_EOBJCORRUPTED;
			goto error;
		}

		name[name_length++] = tolower(c);

	} while ((c = cfg_getchar(cfg, SKIP_COMMENTS)) != ']');

	/*
	 * Here, we enforce that a section name needs to be on its own
	 * line
	 */
	if(cfg_getchar(cfg, SKIP_COMMENTS) != '\n'){
		error = GIT_EOBJCORRUPTED;
		goto error;
	}

	name[name_length] = 0;
	*section_out = name;
	return GIT_SUCCESS;

error:
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
	int error = GIT_SUCCESS;
	char *current_section = NULL;

	/* Initialise the reading position */
	cfg_file->reader.read_ptr = cfg_file->reader.buffer.data;
	cfg_file->reader.eof = 0;

	skip_bom(cfg_file);

	while (error == GIT_SUCCESS && !cfg_file->reader.eof) {

		char *line = cfg_readline(cfg_file);

		/* not enough memory to allocate line */
		if (line == NULL)
			return GIT_ENOMEM;

		strip_comments(line);

		switch (line[0]) {
		case '\0': /* empty line (only whitespace) */
			break;

		case '[': /* section header, new section begins */
			if (current_section)
				free(current_section);
			error = parse_section_header(cfg_file, &current_section, line);
			break;

		default: /* assume variable declaration */
			error = parse_variable(cfg_file, current_section, line);
			cfg_consume_line(cfg_file);
			break;
		}

		free(line);
	}

	if(current_section)
		free(current_section);

	return error;
}

/*
 * Gives $section.$name back, using only name_len chars from the name,
 * which is useful so we don't have to copy the variable name twice.
 * Don't forget to free the memory you get.
 */
static char *build_varname(const char *section, const char *name, int name_len)
{
	char *varname;
	int section_len, ret;
	size_t total_len;

	section_len = strlen(section);
	total_len = section_len + name_len + 2;
	varname = malloc(total_len);
	if(varname == NULL)
		return NULL;

	ret = snprintf(varname, total_len, "%s.%s", section, name);

	return varname;
}

static int parse_variable(git_config *cfg, const char *section_name, const char *line)
{
	int error = GIT_SUCCESS;
	int has_value = 1;
	char *varname;

	const char *var_end = NULL;
	const char *value_start = NULL;

	var_end = strchr(line, '=');

	if (var_end == NULL)
		var_end = strchr(line, '\0');
	else
		value_start = var_end + 1;

	if (isspace(var_end[-1])) {
		do var_end--;
		while (isspace(var_end[0]));
	}

	if (value_start != NULL) {

		while (isspace(value_start[0]))
			value_start++;

		if (value_start[0] == '\0')
			goto error;
	}

	varname = build_varname(section_name, line, var_end - line + 1);
	if(varname == NULL)
		return GIT_ENOMEM;

	config_set(cfg, varname, value_start);

	free(varname);

	return error;

error:
	return GIT_EOBJCORRUPTED;
}
