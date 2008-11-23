#ifndef INCLUDE_git_oid_h__
#define INCLUDE_git_oid_h__

#include "common.h"
#include <string.h>

/**
 * @file git/oid.h
 * @brief Git object id routines
 * @defgroup git_oid Git object id routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Unique identity of any object (commit, tree, blob, tag). */
typedef struct
{
	/** raw binary formatted id */
	unsigned char id[20];
} git_oid;

/**
 * Parse a hex formatted object id into a git_oid.
 * @param out oid structure the result is written into.
 * @param str input hex string; must be pointing at the start of
 *        the hex sequence and have at least the number of bytes
 *        needed for an oid encoded in hex (40 bytes).
 * @return GIT_SUCCESS if valid; GIT_ENOTOID on failure.
 */
GIT_EXTERN(int) git_oid_mkstr(git_oid *out, const char *str);

/**
 * Copy an already raw oid into a git_oid structure.
 * @param out oid structure the result is written into.
 * @param raw the raw input bytes to be copied.
 */
GIT_INLINE(void) git_oid_mkraw(git_oid *out, const unsigned char *raw)
{
	memcpy(out->id, raw, sizeof(out->id));
}

/**
 * Copy an oid from one structure to another.
 * @param out oid structure the result is written into.
 * @param src oid structure to copy from.
 */
GIT_INLINE(void) git_oid_cpy(git_oid *out, const git_oid *src)
{
	memcpy(out->id, src->id, sizeof(out->id));
}

/**
 * Compare two oid structures.
 * @param a first oid structure.
 * @param b second oid structure.
 * @return <0, 0, >0 if a < b, a == b, a > b.
 */
GIT_INLINE(int) git_oid_cmp(const git_oid *a, const git_oid *b)
{
	return memcmp(a->id, b->id, sizeof(a->id));
}

/** @} */
GIT_END_DECL
#endif
