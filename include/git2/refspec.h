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
#ifndef INCLUDE_git_refspec_h__
#define INCLUDE_git_refspec_h__

#include "git2/types.h"

/**
 * @file git2/refspec.h
 * @brief Git refspec attributes
 * @defgroup git_refspec Git refspec attributes
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Get the source specifier
 *
 * @param refspec the refspec
 * @return the refspec's source specifier
 */
const char *git_refspec_src(const git_refspec *refspec);

/**
 * Get the destination specifier
 *
 * @param refspec the refspec
 * @return the refspec's destination specifier
 */
const char *git_refspec_dst(const git_refspec *refspec);

/**
 * Match a refspec's source descriptor with a reference name
 *
 * @param refspec the refspec
 * @param refname the name of the reference to check
 * @return GIT_SUCCESS on successful match; GIT_ENOMACH on match
 * failure or an error code on other failure
 */
int git_refspec_src_match(const git_refspec *refspec, const char *refname);

/**
 * Transform a reference to its target following the refspec's rules
 *
 * @param out where to store the target name
 * @param outlen the size ouf the `out` buffer
 * @param spec the refspec
 * @param name the name of the reference to transform
 * @preturn GIT_SUCCESS, GIT_ESHORTBUFFER or another error
 */
int git_refspec_transform(char *out, size_t outlen, const git_refspec *spec, const char *name);

GIT_END_DECL

#endif
