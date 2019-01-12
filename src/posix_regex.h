/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_posix_regex_h__
#define INCLUDE_posix_regex_h__

#include "common.h"
#include <regex.h>

/*
 * Regular expressions: if the operating system has p_regcomp_l,
 * use that as our p_regcomp implementation, otherwise fall back
 * to standard regcomp.
 */

#define P_REG_EXTENDED REG_EXTENDED
#define P_REG_ICASE REG_ICASE
#define P_REG_NOMATCH REG_NOMATCH

#define p_regex_t regex_t
#define p_regmatch_t regmatch_t

#define p_regerror regerror
#define p_regexec regexec
#define p_regfree regfree

#ifdef GIT_USE_REGCOMP_L
#include <xlocale.h>

GIT_INLINE(int) p_regcomp(p_regex_t *preg, const char *pattern, int cflags)
{
	return regcomp_l(preg, pattern, cflags, (locale_t) 0);
}
#else
# define p_regcomp regcomp
#endif

#endif
