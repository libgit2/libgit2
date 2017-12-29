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
#include "git2/textconv.h"



extern int git_textconv_global_init(void);

extern void git_textconv_free(git_textconv *textconv);

extern int git_textconv__load_ext(
                                     git_textconv **textconv,
                                     git_repository *repo,
                                     git_blob *blob, /* can be NULL */
                                     const char *path);


#endif


