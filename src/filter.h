/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filter_h__
#define INCLUDE_filter_h__

#include "common.h"
#include "array.h"
#include "attr_file.h"
#include "git2/filter.h"

/* Amount of file to examine for NUL byte when checking binary-ness */
#define GIT_FILTER_BYTES_TO_CHECK_NUL 8000

/* Possible CRLF values */
typedef enum {
	GIT_CRLF_GUESS = -1,
	GIT_CRLF_BINARY = 0,
	GIT_CRLF_TEXT,
	GIT_CRLF_INPUT,
	GIT_CRLF_CRLF,
	GIT_CRLF_AUTO,
} git_crlf_t;

typedef struct {
	git_attr_session *attr_session;
	git_buf *temp_buf;
	uint32_t flags;
} git_filter_options;

#define GIT_FILTER_OPTIONS_INIT {0}


typedef struct git_filter_source {
    git_repository *repo;
    const char     *path;
    git_oid         oid;  /* zero if unknown (which is likely) */
    uint16_t        filemode; /* zero if unknown */
    git_filter_mode_t mode;
    uint32_t        flags;
} git_filter_source;

typedef struct git_filter_entry {
    const char *filter_name;
    git_filter *filter;
    void *payload;
} git_filter_entry;

struct git_filter_list {
    git_array_t(git_filter_entry) filters;
    git_filter_source source;
    git_buf *temp_buf;
    char path[GIT_FLEX_ARRAY];
};

extern int git_filter_global_init(void);

extern void git_filter_free(git_filter *filter);

extern int git_filter_list__load_ext(
	git_filter_list **filters,
	git_repository *repo,
	git_blob *blob, /* can be NULL */
	const char *path,
	git_filter_mode_t mode,
	git_filter_options *filter_opts);

extern int git_filter_list_stream_init(
                                git_writestream **out,
                                git_vector *streams,
                                git_filter_list *filters,
                                git_writestream *target);

extern void stream_list_free(git_vector *streams);

/*
 * Available filters
 */

extern git_filter *git_crlf_filter_new(void);
extern git_filter *git_ident_filter_new(void);

#endif
