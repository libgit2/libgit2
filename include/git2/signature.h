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
#ifndef INCLUDE_git_signature_h__
#define INCLUDE_git_signature_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/signature.h
 * @brief Git signature creation
 * @defgroup git_signature Git signature creation
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create a new action signature. The signature must be freed
 * manually or using git_signature_free
 *
 * @param sig_out new signature, in case of error NULL
 * @param name name of the person
 * @param email email of the person
 * @param time time when the action happened
 * @param offset timezone offset in minutes for the time
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_signature_new(git_signature **sig_out, const char *name, const char *email, git_time_t time, int offset);

/**
 * Create a new action signature with a timestamp of 'now'. The
 * signature must be freed manually or using git_signature_free
 *
 * @param sig_out new signature, in case of error NULL
 * @param name name of the person
 * @param email email of the person
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_signature_now(git_signature **sig_out, const char *name, const char *email);


/**
 * Create a copy of an existing signature.
 *
 * All internal strings are also duplicated.
 * @param sig signature to duplicated
 * @return a copy of sig, NULL on out of memory
 */
GIT_EXTERN(git_signature *) git_signature_dup(const git_signature *sig);

/**
 * Free an existing signature
 *
 * @param sig signature to free
 */
GIT_EXTERN(void) git_signature_free(git_signature *sig);

/** @} */
GIT_END_DECL
#endif
