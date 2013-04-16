/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "utf-conv.h"

#define U16_LEAD(c) (wchar_t)(((c)>>10)+0xd7c0)
#define U16_TRAIL(c) (wchar_t)(((c)&0x3ff)|0xdc00)

#if 0
void git__utf8_to_16(wchar_t *dest, size_t length, const char *src)
{
	wchar_t *pDest = dest;
	uint32_t ch;
	const uint8_t* pSrc = (uint8_t*) src;

	assert(dest && src && length);

	length--;

	while(*pSrc && length > 0) {
		ch = *pSrc++;
		length--;

		if(ch < 0xc0) {
			/*
			 * ASCII, or a trail byte in lead position which is treated like
			 * a single-byte sequence for better character boundary
			 * resynchronization after illegal sequences.
			 */
			*pDest++ = (wchar_t)ch;
			continue;
		} else if(ch < 0xe0) { /* U+0080..U+07FF */
			if (pSrc[0]) {
				/* 0x3080 = (0xc0 << 6) + 0x80 */
				*pDest++ = (wchar_t)((ch << 6) + *pSrc++ - 0x3080);
				continue;
			}
		} else if(ch < 0xf0) { /* U+0800..U+FFFF */
			if (pSrc[0] && pSrc[1]) {
				/* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
				/* 0x2080 = (0x80 << 6) + 0x80 */
				ch = (ch << 12) + (*pSrc++ << 6);
				*pDest++ = (wchar_t)(ch + *pSrc++ - 0x2080);
				continue;
			}
		} else /* f0..f4 */ { /* U+10000..U+10FFFF */
			if (length >= 1 && pSrc[0] && pSrc[1] && pSrc[2]) {
				/* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
				ch = (ch << 18) + (*pSrc++ << 12);
				ch += *pSrc++ << 6;
				ch += *pSrc++ - 0x3c82080;
				*(pDest++) = U16_LEAD(ch);
				*(pDest++) = U16_TRAIL(ch);
				length--; /* two bytes for this character */
				continue;
			}
		}

		/* truncated character at the end */
		*pDest++ = 0xfffd;
		break;
	}

	*pDest++ = 0x0;
}
#endif

int git__utf8_to_16(wchar_t *dest, size_t length, const char *src)
{
	return MultiByteToWideChar(CP_UTF8, 0, src, -1, dest, (int)length);
}

int git__utf16_to_8(char *out, const wchar_t *input)
{
	return WideCharToMultiByte(CP_UTF8, 0, input, -1, out, GIT_WIN_PATH, NULL, NULL);
}
