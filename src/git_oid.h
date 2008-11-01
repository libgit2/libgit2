/*
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 * - Neither the name of the Git Development Community nor the
 *   names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef INCLUDE_git_oid_h__
#define INCLUDE_git_oid_h__

#include "git_common.h"

/**
 * @file git_oid.h
 * @brief Git object id routines
 * @defgroup git_oid Git object id routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Unique identity of any object (commit, tree, blob, tag). */
typedef struct
{
	/** raw binary formatted id */
	unsigned char id[20];
} git_oid_t;

/**
 * Parse a hex formatted object id into a git_oid.
 * @param out oid structure the result is written into.
 * @param str input hex string; must be pointing at the start of
 *        the hex sequence and have at least the number of bytes
 *        needed for an oid encoded in hex (40 bytes).
 * @return GIT_SUCCESS if valid; GIT_ENOTOID on failure.
 */
GIT_EXTERN(int) git_oid_mkstr(git_oid_t *out, const char *str);

/**
 * Copy an already raw oid into a git_oid structure.
 * @param out oid structure the result is written into.
 * @param raw the raw input bytes to be copied.
 */
GIT_EXTERN(void) git_oid_mkraw(git_oid_t *out, const unsigned char *raw);

/** @} */
GIT_END_DECL
#endif
