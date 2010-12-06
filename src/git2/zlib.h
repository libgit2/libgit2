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
#ifndef INCLUDE_git_zlib_h__
#define INCLUDE_git_zlib_h__

#include <zlib.h>

/**
 * @file git2/zlib.h
 * @brief Git data compression routines
 * @defgroup git_zlib Git data compression routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

#if defined(NO_DEFLATE_BOUND) || ZLIB_VERNUM < 0x1200
/**
 * deflateBound returns an upper bound on the compressed size.
 *
 * This is a stub function used when zlib does not supply the
 * deflateBound() implementation itself.
 *
 * @param stream the stream pointer.
 * @param s total length of the source data (in bytes).
 * @return maximum length of the compressed data.
 */
GIT_INLINE(size_t) deflateBound(z_streamp stream, size_t s)
{
	return (s + ((s + 7) >> 3) + ((s + 63) >> 6) + 11);
}
#endif

/** @} */
GIT_END_DECL
#endif
