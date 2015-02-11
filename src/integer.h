/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_integer_h__
#define INCLUDE_integer_h__

/** @return true if p fits into the range of a size_t */
GIT_INLINE(int) git__is_sizet(git_off_t p)
{
	size_t r = (size_t)p;
	return p == (git_off_t)r;
}

/** @return true if p fits into the range of an ssize_t */
GIT_INLINE(int) git__is_ssizet(size_t p)
{
	ssize_t r = (ssize_t)p;
	return p == (size_t)r;
}

/** @return true if p fits into the range of a uint32_t */
GIT_INLINE(int) git__is_uint32(size_t p)
{
	uint32_t r = (uint32_t)p;
	return p == (size_t)r;
}

/** @return true if p fits into the range of an unsigned long */
GIT_INLINE(int) git__is_ulong(git_off_t p)
{
	unsigned long r = (unsigned long)p;
	return p == (git_off_t)r;
}

/**
 * Sets `one + two` into `out`, unless the arithmetic would overflow.
 * @return true if the result fits in a `uint64_t`, false on overflow.
 */
GIT_INLINE(bool) git__add_uint64_overflow(uint64_t *out, uint64_t one, uint64_t two)
{
	if (UINT64_MAX - one < two)
		return false;
	if (out)
		*out = one + two;
	return true;
}

/**
 * Sets `one + two` into `out`, unless the arithmetic would overflow.
 * @return true if the result fits in a `size_t`, false on overflow.
 */
GIT_INLINE(bool) git__add_sizet_overflow(size_t *out, size_t one, size_t two)
{
	if (SIZE_MAX - one < two)
		return false;
	if (out)
		*out = one + two;
	return true;
}

/**
 * Sets `one * two` into `out`, unless the arithmetic would overflow.
 * @return true if the result fits in a `size_t`, false on overflow.
 */
GIT_INLINE(bool) git__multiply_sizet_overflow(size_t *out, size_t one, size_t two)
{
	if (one && SIZE_MAX / one < two)
		return false;
	if (out)
		*out = one * two;
	return true;
}

#endif /* INCLUDE_integer_h__ */
