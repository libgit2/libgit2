/*
 * Copyright (c) Edward Thomson.  All rights reserved.
 *
 * This file is part of ntlmclient, distributed under the MIT license.
 * For full terms and copyright information, and for third-party
 * copyright information, see the included LICENSE.txt file.
 */

#ifndef PRIVATE_COMPAT_H__
#define PRIVATE_COMPAT_H__

#if defined (_MSC_VER)
 typedef unsigned char bool;
# ifndef true
#  define true 1
# endif
# ifndef false
#  define false 0
# endif
#else
# include <stdbool.h>
#endif

#ifdef __linux__
/* See man page endian(3) */
# include <endian.h>
# define htonll htobe64
#elif defined(__NetBSD__) || defined(__OpenBSD__)
/* See man page htobe64(3) */
# include <endian.h>
# define htonll htobe64
#elif defined(__FreeBSD__)
/* See man page bwaps64(9) */
# include <sys/endian.h>
# define htonll htobe64
#elif defined(sun) || defined(__sun)
/* See man page byteorder(3SOCKET) */
# include <sys/types.h>
# include <netinet/in.h>
# include <inttypes.h>

# if !defined(htonll)
#  if defined(_BIG_ENDIAN)
#   define htonll(x) (x)
#  else
#   define htonll(x) ((((uint64_t)htonl(x)) << 32) + htonl((uint64_t)(x) >> 32))
#  endif
# endif
#endif

#ifndef MIN
# define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#endif /* PRIVATE_COMPAT_H__ */
