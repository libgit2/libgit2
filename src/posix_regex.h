/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_posix_regex_h__
#define INCLUDE_posix_regex_h__

#include "common.h"

/*
 * Regular expressions: if we were asked to use PCRE (either our
 * bundled version or a system version) then use their regcomp
 * compatible implementation.
 */

#ifdef GIT_REGEX_BUILTIN

# include "pcreposix.h"

# define P_REG_EXTENDED PCRE_REG_EXTENDED
# define P_REG_ICASE PCRE_REG_ICASE
# define P_REG_NOMATCH PCRE_REG_NOMATCH

# define p_regex_t pcre_regex_t
# define p_regmatch_t pcre_regmatch_t
# define p_regcomp pcre_regcomp
# define p_regerror pcre_regerror
# define p_regexec pcre_regexec
# define p_regfree pcre_regfree

/*
 * Use the system-provided `regex` routines, whether that's via the
 * PCRE emulation layer, or libc, preferring `regcomp_l` it's available.
 */

#else

# if defined(GIT_REGEX_PCRE2)
#  include <pcre2posix.h>
# elif defined(GIT_REGEX_PCRE)
#  include <pcreposix.h>
# else
#  include <regex.h>
# endif

# define P_REG_EXTENDED REG_EXTENDED
# define P_REG_ICASE REG_ICASE
# define P_REG_NOMATCH REG_NOMATCH

# define p_regex_t regex_t
# define p_regmatch_t regmatch_t

# define p_regerror regerror
# define p_regexec regexec
# define p_regfree regfree

# ifdef GIT_REGEX_REGCOMP_L
#  include <xlocale.h>

GIT_INLINE(int) p_regcomp(p_regex_t *preg, const char *pattern, int cflags)
{
	return regcomp_l(preg, pattern, cflags, (locale_t) 0);
}

# else
#  define p_regcomp regcomp
# endif /* GIT_REGEX_REGCOMP_L */

#endif

#endif
