/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_object_h__
#define INCLUDE_object_h__

#include "common.h"
#include "array.h"

/** Base git object for inheritance */
struct git_object {
	git_cached_obj cached;
	git_repository *repo;
};

/* fully free the object; internal method, DO NOT EXPORT */
void git_object__free(void *object);

int git_object__from_odb_object(
	git_object **out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_otype type,
	bool lax);

int git_object__resolve_to_type(git_object **obj, git_otype type);

void git_oid__writebuf(git_buf *buf, const char *header, const git_oid *oid);

enum {
	GIT_PARSE_BODY_OPTIONAL = -2,
	GIT_PARSE_BODY = -1,
	GIT_PARSE_MODE_OPTIONAL = 0,
	GIT_PARSE_OID = 1,
	GIT_PARSE_OID_ARRAY = 2,
	GIT_PARSE_OTYPE = 3,
	GIT_PARSE_SIGNATURE = 4,
	GIT_PARSE_TO_EOL = 5,
};

typedef git_array_t(git_oid) git_oid_array;

typedef struct {
	const char *tag;
	size_t taglen;
	int type;
	union {
		git_oid *id;
		git_otype *otype;
		char **text;
		git_signature **sig;
		git_oid_array *ids;
		const char **body;
	} value;
} git_object_parse_t;

/* parse tagged lines followed by blank line and message body */
int git_object__parse_lines(
	git_otype type,
	git_object_parse_t *parse,
	const char *buf,
	const char *buf_end);

#endif

