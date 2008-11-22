#ifndef INCLUDE_git_zlib_h__
#define INCLUDE_git_zlib_h__

#include "common.h"
#include <zlib.h>

/**
 * @file git/zlib.h
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
