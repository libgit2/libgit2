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

#if defined(NTLM_BIG_ENDIAN)
static inline uint64_t ntlm_htonll(uint64_t value)
{
	return value;
}
#else

# include <arpa/inet.h>

static inline uint64_t ntlm_htonll(uint64_t value)
{
	return ((uint64_t)htonl(value) << 32) + htonl((uint64_t)value >> 32);
}

#endif

#ifndef MIN
# define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#endif /* PRIVATE_COMPAT_H__ */
