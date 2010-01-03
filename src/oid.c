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

#include "common.h"
#include "git/oid.h"
#include <string.h>

static signed char from_hex[] = {
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 00 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 10 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 20 */
 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1, /* 30 */
-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 40 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 50 */
-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 60 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 70 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 80 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 90 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* a0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* b0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* c0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* d0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* e0 */
-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* f0 */
};
static char to_hex[] = "0123456789abcdef";

int git_oid_mkstr(git_oid *out, const char *str)
{
	size_t p;
	for (p = 0; p < sizeof(out->id); p++, str += 2) {
		int v = (from_hex[(unsigned char)str[0]] << 4)
		       | from_hex[(unsigned char)str[1]];
		if (v < 0)
			return GIT_ENOTOID;
		out->id[p] = (unsigned char)v;
	}
	return GIT_SUCCESS;
}

GIT_INLINE(char) *fmt_one(char *str, unsigned int val)
{
	*str++ = to_hex[val >> 4];
	*str++ = to_hex[val & 0xf];
	return str;
}

void git_oid_fmt(char *str, const git_oid *oid)
{
	size_t i;

	for (i = 0; i < sizeof(oid->id); i++)
		str = fmt_one(str, oid->id[i]);
}

void git_oid_pathfmt(char *str, const git_oid *oid)
{
	size_t i;

	str = fmt_one(str, oid->id[0]);
	*str++ = '/';
	for (i = 1; i < sizeof(oid->id); i++)
		str = fmt_one(str, oid->id[i]);
}

char *git_oid_allocfmt(const git_oid *oid)
{
	char *str = git__malloc(GIT_OID_HEXSZ + 1);
	if (!str)
		return NULL;
	git_oid_fmt(str, oid);
	str[GIT_OID_HEXSZ] = '\0';
	return str;
}

char *git_oid_to_string(char *out, size_t n, const git_oid *oid)
{
	char str[GIT_OID_HEXSZ];

	if (!out || n == 0 || !oid)
		return "";

	n--;  /* allow room for terminating NUL */

	if (n > 0) {
		git_oid_fmt(str, oid);
		memcpy(out, str, n > GIT_OID_HEXSZ ? GIT_OID_HEXSZ : n);
	}

	out[n] = '\0';

	return out;
}

