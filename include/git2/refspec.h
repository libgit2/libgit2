/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_refspec_h__
#define INCLUDE_git_refspec_h__

#include "types.h"

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
 * @return GIT_SUCCESS, GIT_ESHORTBUFFER or another error
 */
int git_refspec_transform(char *out, size_t outlen, const git_refspec *spec, const char *name);

GIT_END_DECL

#endif
