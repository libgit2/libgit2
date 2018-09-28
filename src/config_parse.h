/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_config_parse_h__
#define INCLUDE_config_parse_h__

#include "common.h"
#include "array.h"
#include "oid.h"
#include "parse.h"

extern const char *git_config_escapes;
extern const char *git_config_escaped;

typedef struct config_file {
	git_oid checksum;
	char *path;
	git_array_t(struct config_file) includes;
} git_config_file;

typedef struct {
	struct config_file *file;
	git_parse_ctx ctx;
} git_config_parser;

typedef int (*git_config_parser_section_cb)(
	git_config_parser *parser,
	const char *current_section,
	const char *line,
	size_t line_len,
	void *data);

typedef int (*git_config_parser_variable_cb)(
	git_config_parser *parser,
	const char *current_section,
	const char *var_name,
	const char *var_value,
	const char *line,
	size_t line_len,
	void *data);

typedef int (*git_config_parser_comment_cb)(
	git_config_parser *parser,
	const char *line,
	size_t line_len,
	void *data);

typedef int (*git_config_parser_eof_cb)(
	git_config_parser *parser,
	const char *current_section,
	void *data);

int git_config_parse(
	git_config_parser *parser,
	git_config_parser_section_cb on_section,
	git_config_parser_variable_cb on_variable,
	git_config_parser_comment_cb on_comment,
	git_config_parser_eof_cb on_eof,
	void *data);

#endif
