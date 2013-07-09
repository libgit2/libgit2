/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_bitvec_h__
#define INCLUDE_bitvec_h__

#include "util.h"

/*
 * This is a silly little fixed length bit vector type that will store
 * vectors of 64 bits or less directly in the structure and allocate
 * memory for vectors longer than 64 bits.  You can use the two versions
 * transparently through the API and avoid heap allocation completely when
 * using a short bit vector as a result.
 */
typedef struct {
	size_t length;
	union {
		uint8_t *ptr;
		uint64_t bits;
	} u;
} git_bitvec;

GIT_INLINE(int) git_bitvec_init(git_bitvec *bv, size_t capacity)
{
	if (capacity < 64) {
		bv->length = 0;
		bv->u.bits = 0;
		return 0;
	}

	bv->length = (capacity + 7) / 8;
	bv->u.ptr  = git__calloc(bv->length, 1);
	return bv->u.ptr ? 0 : -1;
}

#define GIT_BITVEC_MASK_INLINE(BIT) (((uint64_t)1) << BIT)

#define GIT_BITVEC_MASK_BYTE(BIT)  (((uint8_t)1) << ((BIT) & 0x07))
#define GIT_BITVEC_INDEX_BYTE(BIT) ((BIT) >> 3)

GIT_INLINE(void) git_bitvec_set(git_bitvec *bv, size_t bit, bool on)
{
	if (!bv->length) {
		assert(bit < 64);

		if (on)
			bv->u.bits |= GIT_BITVEC_MASK_INLINE(bit);
		else
			bv->u.bits &= ~GIT_BITVEC_MASK_INLINE(bit);
	} else {
		assert(bit < bv->length * 8);

		if (on)
			bv->u.ptr[GIT_BITVEC_INDEX_BYTE(bit)] |= GIT_BITVEC_MASK_BYTE(bit);
		else
			bv->u.ptr[GIT_BITVEC_INDEX_BYTE(bit)] &= ~GIT_BITVEC_MASK_BYTE(bit);
	}
}

GIT_INLINE(bool) git_bitvec_get(git_bitvec *bv, size_t bit)
{
	if (!bv->length) {
		assert(bit < 64);
		return (bv->u.bits & GIT_BITVEC_MASK_INLINE(bit)) != 0;
	} else {
		assert(bit < bv->length * 8);
		return (bv->u.ptr[GIT_BITVEC_INDEX_BYTE(bit)] &
				GIT_BITVEC_MASK_BYTE(bit)) != 0;
	}
}

GIT_INLINE(void) git_bitvec_clear(git_bitvec *bv)
{
	if (!bv->length)
		bv->u.bits = 0;
	else
		memset(bv->u.ptr, 0, bv->length);
}

GIT_INLINE(void) git_bitvec_free(git_bitvec *bv)
{
	if (bv->length) {
		git__free(bv->u.ptr);
		memset(bv, 0, sizeof(*bv));
	}
}

#endif
