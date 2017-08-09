/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>

#include "sha1_lookup.h"
#include "common.h"
#include "oid.h"

int sha1_position(const void *table,
			size_t stride,
			unsigned lo, unsigned hi,
			const unsigned char *key)
{
	const unsigned char *base = table;

	do {
		unsigned mi = (lo + hi) / 2;
		int cmp = git_oid__hashcmp(base + mi * stride, key);

		if (!cmp)
			return mi;

		if (cmp > 0)
			hi = mi;
		else
			lo = mi+1;
	} while (lo < hi);

	return -((int)lo)-1;
}
