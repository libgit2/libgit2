/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_oid_h__
#define INCLUDE_oid_h__

#include "common.h"

#include "git2/oid.h"

extern const git_oid git_oid__empty_blob_sha1;
extern const git_oid git_oid__empty_tree_sha1;

/**
 * Format a git_oid into a newly allocated c-string.
 *
 * The c-string is owned by the caller and needs to be manually freed.
 *
 * @param id the oid structure to format
 * @return the c-string; NULL if memory is exhausted. Caller must
 *			deallocate the string with git__free().
 */
char *git_oid_allocfmt(const git_oid *id);

GIT_INLINE(int) git_oid_raw_ncmp(
	const unsigned char *sha1,
	const unsigned char *sha2,
	size_t len)
{
	if (len > GIT_OID_HEXSZ)
		len = GIT_OID_HEXSZ;

	while (len > 1) {
		if (*sha1 != *sha2)
			return 1;
		sha1++;
		sha2++;
		len -= 2;
	};

	if (len)
		if ((*sha1 ^ *sha2) & 0xf0)
			return 1;

	return 0;
}

GIT_INLINE(int) git_oid_raw_cmp(
	const unsigned char *sha1,
	const unsigned char *sha2)
{
	return memcmp(sha1, sha2, GIT_OID_RAWSZ);
}

GIT_INLINE(int) git_oid_raw_cpy(
	unsigned char *dst,
	const unsigned char *src)
{
	memcpy(dst, src, GIT_OID_RAWSZ);
	return 0;
}

/*
 * Compare two oid structures.
 *
 * @param a first oid structure.
 * @param b second oid structure.
 * @return <0, 0, >0 if a < b, a == b, a > b.
 */
GIT_INLINE(int) git_oid__cmp(const git_oid *a, const git_oid *b)
{
	return git_oid_raw_cmp(a->id, b->id);
}

GIT_INLINE(void) git_oid__cpy_prefix(
	git_oid *out, const git_oid *id, size_t len)
{
	memcpy(&out->id, id->id, (len + 1) / 2);

	if (len & 1)
		out->id[len / 2] &= 0xF0;
}

GIT_INLINE(bool) git_oid__is_hexstr(const char *str)
{
	size_t i;

	for (i = 0; str[i] != '\0'; i++) {
		if (git__fromhex(str[i]) < 0)
			return false;
	}

	return (i == GIT_OID_HEXSZ);
}

#endif
