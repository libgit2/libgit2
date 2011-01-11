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
#ifndef INCLUDE_git_compat_h__
#define INCLUDE_git_compat_h__

/**
 * @file git2/compat.h
 * @brief Type compatibility layer necessary for clients of the library.
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

// NOTE: This needs to be in a public header so that both the library
// implementation and client applications both agree on the same types.
// Otherwise we get undefined behavior.
//
// Use the "best" types that each platform provides. Currently we truncate
// these intermediate representations for compatibility with the git ABI, but
// if and when it changes to support 64 bit types, our code will naturally
// adapt.
//
// NOTE: These types should match those that are returned by our internal
// stat() functions, for all platforms.
#if defined(_MSC_VER)

typedef __int64 git_off_t;
typedef __time64_t git_time_t;

#elif defined(__MINGW32__)

typedef off64_t git_off_t;
typedef time_t git_time_t;

#else  // POSIX

// Note: Can't use off_t since if a client program includes <sys/types.h>
// before us (directly or indirectly), they'll get 32 bit off_t in their client
// app, even though /we/ define _FILE_OFFSET_BITS=64.
typedef long long git_off_t;
typedef time_t git_time_t;

#endif

/** @} */
GIT_END_DECL

#endif
