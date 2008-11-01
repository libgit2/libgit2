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

#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "git/common.h"
#include "git/oid.h"
#include <time.h>

/**
 * @file git/commit.h
 * @brief Git commit parsing, formatting routines
 * @defgroup git_commit Git commit parsing, formatting routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Parsed representation of a commit object. */
typedef struct git_commit git_commit;
#ifdef GIT__PRIVATE
struct git_commit {
	git_oid id;
	time_t commit_time;
	unsigned parsed:1,
	         flags:26;
};
#endif

/**
 * Parse (or lookup) a commit from a revision pool.
 * @param pool the pool to use when parsing/caching the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return the commit; NULL if the commit does not exist in the
 *         pool's git_odb, or if the commit is present but is
 *         too malformed to be parsed successfully.
 */
GIT_EXTERN(git_commit*) git_commit_parse(git_revp *pool, const git_oid *id);

/**
 * Get the id of a commit.
 * @param commit a previously parsed commit.
 * @return object identity for the commit.
 */
GIT_EXTERN(const git_oid*) git_commit_id(git_commit *commit);

/** @} */
GIT_END_DECL
#endif
