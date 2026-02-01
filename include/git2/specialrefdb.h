/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_specialrefdb_h__
#define INCLUDE_git_specialrefdb_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/sys/specialrefdb.h
 * @brief A database for special references (HEAD, MERGE_HEAD, etc)
 * @defgroup git_specialrefdbb A database for special references (HEAD, MERGE_HEAD, etc)
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

GIT_EXTERN(int) git_specialrefdb_open(git_specialrefdb **out, git_repository *repo);

/** @} */
GIT_END_DECL

#endif
