/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_mailmap_h__
#define INCLUDE_mailmap_h__

#include "common.h"
#include "repository.h"

/**
 * @file git2/mailmap.h
 * @brief Mailmap access subroutines.
 * @defgroup git_rebase Git merge routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct git_mailmap git_mailmap;

struct git_mailmap_entry {
	const char* name;
	const char* email;
};

GIT_EXTERN(int) git_mailmap_create(git_mailmap**, git_repository*);
GIT_EXTERN(void) git_mailmap_free(git_mailmap*);
GIT_EXTERN(struct git_mailmap_entry) git_mailmap_lookup(
	git_mailmap* map,
	const char* name,
	const char* email);

/** @} */
GIT_END_DECL

#endif
