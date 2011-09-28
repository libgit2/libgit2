/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_msvc_compat__
#define INCLUDE_msvc_compat__

#if defined(_MSC_VER)

/*
 * Disable silly MSVC warnings
 */

/* conditional expression is constant */
#pragma warning(disable: 4127) 

/* nonstandard extension used : bit field types other than int */
#pragma warning(disable: 4214)

/* access() mode parameter #defines	*/
# define F_OK 0 /* existence check */
# define W_OK 2 /* write mode check */
# define R_OK 4 /* read mode check */

# define lseek _lseeki64
# define stat _stat64
# define fstat _fstat64

/* stat: file mode type testing macros */
# define _S_IFLNK 0120000
# define S_IFLNK _S_IFLNK

# define S_ISDIR(m)	(((m) & _S_IFMT) == _S_IFDIR)
# define S_ISREG(m)	(((m) & _S_IFMT) == _S_IFREG)
# define S_ISFIFO(m) (((m) & _S_IFMT) == _S_IFIFO)
# define S_ISLNK(m) (((m) & _S_IFMT) == _S_IFLNK)

# define mode_t unsigned short

/* case-insensitive string comparison */
# define strcasecmp	_stricmp
# define strncasecmp _strnicmp

#if (_MSC_VER >= 1600)
#	include <stdint.h>
#else
/* add some missing <stdint.h> typedef's */
typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef short int16_t;
typedef unsigned short uint16_t;

typedef long int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef long long intmax_t;
typedef unsigned long long uintmax_t;
#endif

#endif

#endif /* INCLUDE_msvc_compat__ */
