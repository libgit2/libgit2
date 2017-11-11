/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "config_parse.h"

#include "buf_text.h"

#include <ctype.h>

static void set_parse_error(git_config_parser *reader, int col, const char *error_str)
{
	giterr_set(GITERR_CONFIG, "failed to parse config file: %s (in %s:%d, column %d)",
		error_str, reader->file->path, reader->line_number, col);
}

static int reader_getchar_raw(git_config_parser *reader)
{
	int c;

	c = *reader->read_ptr++;

	/*
	Win 32 line breaks: if we find a \r\n sequence,
	return only the \n as a newline
	*/
	if (c == '\r' && *reader->read_ptr == '\n') {
		reader->read_ptr++;
		c = '\n';
	}

	if (c == '\n')
		reader->line_number++;

	if (c == 0) {
		reader->eof = 1;
		c = '\0';
	}

	return c;
}

#define SKIP_WHITESPACE (1 << 1)
#define SKIP_COMMENTS (1 << 2)

static int reader_getchar(git_config_parser *reader, int flags)
{
	const int skip_whitespace = (flags & SKIP_WHITESPACE);
	const int skip_comments = (flags & SKIP_COMMENTS);
	int c;

	assert(reader->read_ptr);

	do {
		c = reader_getchar_raw(reader);
	} while (c != '\n' && c != '\0' && skip_whitespace && git__isspace(c));

	if (skip_comments && (c == '#' || c == ';')) {
		do {
			c = reader_getchar_raw(reader);
		} while (c != '\n' && c != '\0');
	}

	return c;
}

/*
 * Read the next char, but don't move the reading pointer.
 */
static int reader_peek(git_config_parser *reader, int flags)
{
	void *old_read_ptr;
	int old_lineno, old_eof;
	int ret;

	assert(reader->read_ptr);

	old_read_ptr = reader->read_ptr;
	old_lineno = reader->line_number;
	old_eof = reader->eof;

	ret = reader_getchar(reader, flags);

	reader->read_ptr = old_read_ptr;
	reader->line_number = old_lineno;
	reader->eof = old_eof;

	return ret;
}

/*
 * Read and consume a line, returning it in newly-allocated memory.
 */
static char *reader_readline(git_config_parser *reader, bool skip_whitespace)
{
	char *line = NULL;
	char *line_src, *line_end;
	size_t line_len, alloc_len;

	line_src = reader->read_ptr;

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

	if (GIT_ADD_SIZET_OVERFLOW(&alloc_len, line_len, 1) ||
		(line = git__malloc(alloc_len)) == NULL) {
		return NULL;
	}

	memcpy(line, line_src, line_len);

	do line[line_len] = '\0';
	while (line_len-- > 0 && git__isspace(line[line_len]));

	if (*line_end == '\n')
		line_end++;

	if (*line_end == '\0')
		reader->eof = 1;

	reader->line_number++;
	reader->read_ptr = line_end;

	return line;
}

/*
 * Consume a line, without storing it anywhere
 */
static void reader_consume_line(git_config_parser *reader)
{
	char *line_start, *line_end;

	line_start = reader->read_ptr;
	line_end = strchr(line_start, '\n');
	/* No newline at EOF */
	if(line_end == NULL){
		line_end = strchr(line_start, '\0');
	}

	if (*line_end == '\n')
		line_end++;

	if (*line_end == '\0')
		reader->eof = 1;

	reader->line_number++;
	reader->read_ptr = line_end;
}

GIT_INLINE(int) config_keychar(int c)
{
	return isalnum(c) || c == '-';
}

static int strip_comments(char *line, int in_quotes)
{
	int quote_count = in_quotes, backslash_count = 0;
	char *ptr;

	for (ptr = line; *ptr; ++ptr) {
		if (ptr[0] == '"' && ptr > line && ptr[-1] != '\\')
			quote_count++;

		if ((ptr[0] == ';' || ptr[0] == '#') &&
			(quote_count % 2) == 0 &&
			(backslash_count % 2) == 0) {
			ptr[0] = '\0';
			break;
		}

		if (ptr[0] == '\\')
			backslash_count++;
		else
			backslash_count = 0;
	}

	/* skip any space at the end */
	while (ptr > line && git__isspace(ptr[-1])) {
		ptr--;
	}
	ptr[0] = '\0';

	return quote_count;
}


static int parse_section_header_ext(git_config_parser *reader, const char *line, const char *base_name, char **section_name)
{
	int c, rpos;
	char *first_quote, *last_quote;
	git_buf buf = GIT_BUF_INIT;
	size_t quoted_len, alloc_len, base_name_len = strlen(base_name);

	/*
	 * base_name is what came before the space. We should be at the
	 * first quotation mark, except for now, line isn't being kept in
	 * sync so we only really use it to calculate the length.
	 */

	first_quote = strchr(line, '"');
	if (first_quote == NULL) {
		set_parse_error(reader, 0, "Missing quotation marks in section header");
		goto end_error;
	}

	last_quote = strrchr(line, '"');
	quoted_len = last_quote - first_quote;

	if (quoted_len == 0) {
		set_parse_error(reader, 0, "Missing closing quotation mark in section header");
		goto end_error;
	}

	GITERR_CHECK_ALLOC_ADD(&alloc_len, base_name_len, quoted_len);
	GITERR_CHECK_ALLOC_ADD(&alloc_len, alloc_len, 2);

	if (git_buf_grow(&buf, alloc_len) < 0 ||
	    git_buf_printf(&buf, "%s.", base_name) < 0)
		goto end_error;

	rpos = 0;

	line = first_quote;
	c = line[++rpos];

	/*
	 * At the end of each iteration, whatever is stored in c will be
	 * added to the string. In case of error, jump to out
	 */
	do {

		switch (c) {
		case 0:
			set_parse_error(reader, 0, "Unexpected end-of-line in section header");
			goto end_error;

		case '"':
			goto end_parse;

		case '\\':
			c = line[++rpos];

			if (c == 0) {
				set_parse_error(reader, rpos, "Unexpected end-of-line in section header");
				goto end_error;
			}

		default:
			break;
		}

		git_buf_putc(&buf, (char)c);
		c = line[++rpos];
	} while (line + rpos < last_quote);

end_parse:
	if (git_buf_oom(&buf))
		goto end_error;

	if (line[rpos] != '"' || line[rpos + 1] != ']') {
		set_parse_error(reader, rpos, "Unexpected text after closing quotes");
		git_buf_free(&buf);
		return -1;
	}

	*section_name = git_buf_detach(&buf);
	return 0;

end_error:
	git_buf_free(&buf);

	return -1;
}

static int parse_section_header(git_config_parser *reader, char **section_out)
{
	char *name, *name_end;
	int name_length, c, pos;
	int result;
	char *line;
	size_t line_len;

	line = reader_readline(reader, true);
	if (line == NULL)
		return -1;

	/* find the end of the variable's name */
	name_end = strrchr(line, ']');
	if (name_end == NULL) {
		git__free(line);
		set_parse_error(reader, 0, "Missing ']' in section header");
		return -1;
	}

	GITERR_CHECK_ALLOC_ADD(&line_len, (size_t)(name_end - line), 1);
	name = git__malloc(line_len);
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
			result = parse_section_header_ext(reader, line, name, section_out);
			git__free(line);
			git__free(name);
			return result;
		}

		if (!config_keychar(c) && c != '.') {
			set_parse_error(reader, pos, "Unexpected character in header");
			goto fail_parse;
		}

		name[name_length++] = (char)git__tolower(c);

	} while ((c = line[pos++]) != ']');

	if (line[pos - 1] != ']') {
		set_parse_error(reader, pos, "Unexpected end of file");
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

static int skip_bom(git_config_parser *reader)
{
	git_bom_t bom;
	int bom_offset = git_buf_text_detect_bom(&bom,
		&reader->buffer, reader->read_ptr - reader->buffer.ptr);

	if (bom == GIT_BOM_UTF8)
		reader->read_ptr += bom_offset;

	/* TODO: reference implementation is pretty stupid with BoM */

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

/* '\"' -> '"' etc */
static int unescape_line(
	char **out, bool *is_multi, const char *ptr, int quote_count)
{
	char *str, *fixed, *esc;
	size_t ptr_len = strlen(ptr), alloc_len;

	*is_multi = false;

	if (GIT_ADD_SIZET_OVERFLOW(&alloc_len, ptr_len, 1) ||
		(str = git__malloc(alloc_len)) == NULL) {
		return -1;
	}

	fixed = str;

	while (*ptr != '\0') {
		if (*ptr == '"') {
			quote_count++;
		} else if (*ptr != '\\') {
			*fixed++ = *ptr;
		} else {
			/* backslash, check the next char */
			ptr++;
			/* if we're at the end, it's a multiline, so keep the backslash */
			if (*ptr == '\0') {
				*is_multi = true;
				goto done;
			}
			if ((esc = strchr(git_config_escapes, *ptr)) != NULL) {
				*fixed++ = git_config_escaped[esc - git_config_escapes];
			} else {
				git__free(str);
				giterr_set(GITERR_CONFIG, "invalid escape at %s", ptr);
				return -1;
			}
		}
		ptr++;
	}

done:
	*fixed = '\0';
	*out = str;

	return 0;
}

static int parse_multiline_variable(git_config_parser *reader, git_buf *value, int in_quotes)
{
	char *line = NULL, *proc_line = NULL;
	int quote_count;
	bool multiline;

	/* Check that the next line exists */
	line = reader_readline(reader, false);
	if (line == NULL)
		return -1;

	/* We've reached the end of the file, there is no continuation.
	 * (this is not an error).
	 */
	if (line[0] == '\0') {
		git__free(line);
		return 0;
	}

	quote_count = strip_comments(line, !!in_quotes);

	/* If it was just a comment, pretend it didn't exist */
	if (line[0] == '\0') {
		git__free(line);
		return parse_multiline_variable(reader, value, quote_count);
		/* TODO: unbounded recursion. This **could** be exploitable */
	}

	if (unescape_line(&proc_line, &multiline, line, in_quotes) < 0) {
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
	if (multiline)
		return parse_multiline_variable(reader, value, quote_count);

	return 0;
}

GIT_INLINE(bool) is_namechar(char c)
{
	return isalnum(c) || c == '-';
}

static int parse_name(
	char **name, const char **value, git_config_parser *reader, const char *line)
{
	const char *name_end = line, *value_start;

	*name = NULL;
	*value = NULL;

	while (*name_end && is_namechar(*name_end))
		name_end++;

	if (line == name_end) {
		set_parse_error(reader, 0, "Invalid configuration key");
		return -1;
	}

	value_start = name_end;

	while (*value_start && git__isspace(*value_start))
		value_start++;

	if (*value_start == '=') {
		*value = value_start + 1;
	} else if (*value_start) {
		set_parse_error(reader, 0, "Invalid configuration key");
		return -1;
	}

	if ((*name = git__strndup(line, name_end - line)) == NULL)
		return -1;

	return 0;
}

static int parse_variable(git_config_parser *reader, char **var_name, char **var_value)
{
	const char *value_start = NULL;
	char *line;
	int quote_count;
	bool multiline;

	line = reader_readline(reader, true);
	if (line == NULL)
		return -1;

	quote_count = strip_comments(line, 0);

	/* If there is no value, boolean true is assumed */
	*var_value = NULL;

	if (parse_name(var_name, &value_start, reader, line) < 0)
		goto on_error;

	/*
	 * Now, let's try to parse the value
	 */
	if (value_start != NULL) {
		while (git__isspace(value_start[0]))
			value_start++;

		if (unescape_line(var_value, &multiline, value_start, 0) < 0)
			goto on_error;

		if (multiline) {
			git_buf multi_value = GIT_BUF_INIT;
			git_buf_attach(&multi_value, *var_value, 0);

			if (parse_multiline_variable(reader, &multi_value, quote_count) < 0 ||
				git_buf_oom(&multi_value)) {
				git_buf_free(&multi_value);
				goto on_error;
			}

			*var_value = git_buf_detach(&multi_value);
		}
	}

	git__free(line);
	return 0;

on_error:
	git__free(*var_name);
	git__free(line);
	return -1;
}

int git_config_parse(
	git_config_parser *parser,
	git_config_parser_section_cb on_section,
	git_config_parser_variable_cb on_variable,
	git_config_parser_comment_cb on_comment,
	git_config_parser_eof_cb on_eof,
	void *data)
{
	char *current_section = NULL, *var_name, *var_value, *line_start;
	char c;
	size_t line_len;
	int result = 0;

	skip_bom(parser);

	while (result == 0 && !parser->eof) {
		line_start = parser->read_ptr;

		c = reader_peek(parser, SKIP_WHITESPACE);

		switch (c) {
		case '\0': /* EOF when peeking, set EOF in the parser to exit the loop */
			parser->eof = 1;
			break;

		case '[': /* section header, new section begins */
			git__free(current_section);
			current_section = NULL;

			if ((result = parse_section_header(parser, &current_section)) == 0 && on_section) {
				line_len = parser->read_ptr - line_start;
				result = on_section(parser, current_section, line_start, line_len, data);
			}
			break;

		case '\n': /* comment or whitespace-only */
		case ';':
		case '#':
			reader_consume_line(parser);

			if (on_comment) {
				line_len = parser->read_ptr - line_start;
				result = on_comment(parser, line_start, line_len, data);
			}
			break;

		default: /* assume variable declaration */
			if ((result = parse_variable(parser, &var_name, &var_value)) == 0 && on_variable) {
				line_len = parser->read_ptr - line_start;
				result = on_variable(parser, current_section, var_name, var_value, line_start, line_len, data);
			}
			break;
		}
	}

	if (on_eof)
		result = on_eof(parser, current_section, data);

	git__free(current_section);
	return result;
}
