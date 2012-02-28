/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_filter_h__
#define INCLUDE_filter_h__

#include "common.h"
#include "buffer.h"
#include "git2/odb.h"
#include "git2/repository.h"

typedef int (*git_filter_cb)(git_buf *dest, const git_buf *source, const char *filename);

typedef enum {
	GIT_FILTER_TO_WORKTREE,
	GIT_FILTER_TO_ODB
} git_filter_mode;

typedef enum {
	GIT_CRLF_GUESS = -1,
	GIT_CRLF_BINARY = 0,
	GIT_CRLF_TEXT,
	GIT_CRLF_INPUT,
	GIT_CRLF_CRLF,
	GIT_CRLF_AUTO,

	GIT_SAFE_CRLF_FALSE = 0,
	GIT_SAFE_CRLF_FAIL = 1,
	GIT_SAFE_CRLF_WARN = 2,

	GIT_AUTO_CRLF_FALSE = 0,
	GIT_AUTO_CRLF_TRUE = 1,
	GIT_AUTO_CRLF_INPUT = -1,
} git_crlf_t;

typedef enum {
	GIT_EOL_UNSET,
	GIT_EOL_CRLF,
	GIT_EOL_LF,
#ifdef GIT_WIN32
	GIT_EOL_NATIVE = GIT_EOL_CRLF
#else
	GIT_EOL_NATIVE = GIT_EOL_LF
#endif
} git_eol_t;


typedef struct {
	int crlf_action;
	int eol_attr;
	int ident;
} git_conv_attrs;

typedef struct {
	/* NUL, CR, LF and CRLF counts */
	unsigned int nul, cr, lf, crlf;

	/* These are just approximations! */
	unsigned int printable, nonprintable;
} git_text_stats;

extern int git_filter__load_for_file(git_vector *filters, git_repository *repo, const char *full_path, int mode);
extern int git_filter__load_attrs(git_conv_attrs *ca, git_repository *repo, const char *path);
extern int git_filter__apply(git_buf *dest, git_buf *source, git_vector *filters, const char *filename);

/* Gather stats for a piece of text */
extern void git_text__stat(git_text_stats *stats, git_buf *text);

/* Heuristics on a set of text stats to check whether it's binary
 * text or not */
extern int git_text__is_binary(git_text_stats *stats);

#endif
