/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "base64.h"

/*
 * This is a modified version of base64_encode as found in Gnulib:
 * https://www.gnu.org/software/gnulib
 */

static const char b64str[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* C89 compliant way to cast 'char' to 'unsigned char'. */
static inline unsigned char to_uchar(char ch)
{
	return ch;
}

/*
 * Base64 encode IN array of size INLEN into OUT array of size OUTLEN.
 * If OUTLEN is less than BASE64_LENGTH(INLEN), write as many bytes as
 * possible.  If OUTLEN is larger than BASE64_LENGTH(INLEN), also zero
 * terminate the output buffer.
 */
int git_base64_encode(char *out, size_t outlen, const char *in, size_t inlen)
{
	while (inlen && outlen) {
		*out++ = b64str[(to_uchar (in[0]) >> 2) & 0x3f];
		if (!--outlen)
			break;

		*out++ = b64str[((to_uchar (in[0]) << 4)
			+ (--inlen ? to_uchar (in[1]) >> 4 : 0)) & 0x3f];
		if (!--outlen)
			break;

		*out++ = (inlen ? b64str[((to_uchar (in[1]) << 2)
			+ (--inlen ? to_uchar (in[2]) >> 6 : 0)) & 0x3f] : '=');
		if (!--outlen)
			break;

		*out++ = inlen ? b64str[to_uchar (in[2]) & 0x3f] : '=';
		if (!--outlen)
			break;

		if (inlen)
			inlen--;
		if (inlen)
			in += 3;
	}

	if (outlen)
		*out = '\0';
	return 0;
}
