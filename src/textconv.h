/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_textconv_h__
#define INCLUDE_textconv_h__

#include "common.h"

#include "attr_file.h"
#include "diff_driver.h"


/**
 * A textconv that can transform file data
 *
 * This represents a textconv that can be used to transform or even replace
 * file data. Libgit2 includes one built in textconv and it is possible to
 * write your own (see git2/sys/textconv.h for information on that).
 */
typedef struct git_textconv git_textconv;

extern int git_textconv_global_init(void);

/**
 * Load the textconv from the given driver.
 *
 * This will return 0 (success) but set the output git_textconv to NULL
 * if no textconvs are requested for the given file.
 *
 * @param textconv Output reference to a git_textconv (or NULL)
 * @param driver Diff driver object for file
 * @return 0 on success (which could still return NULL if no textconvs are
 *         needed for the requested file), <0 on error
 */
extern int git_textconv_load(
	git_textconv **textconv,
	git_diff_driver* driver);

/**
 * Free a git_textconv_list
 *
 * @param textconv A git_textconv created by `git_textconv_load`
 */
extern void git_textconv_free(git_textconv *textconv);

/**
 * Apply a textconv to a a stream
 *
 * @param out point to resulting stream
 * @param textconv the textconv to apply
 * @param temp_buf a buffer to hold data
 * @param target the stream into which the data will be written
 */
extern int git_textconv_init_stream(
	git_writestream** out,
	git_textconv *textconv,
	git_buf* temp_buf,
	git_writestream *target);

#endif


