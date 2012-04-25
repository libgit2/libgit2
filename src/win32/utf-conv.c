/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "utf-conv.h"
#include "git2/windows.h"

/*
 * Default codepage value
 */
static int _active_codepage = CP_UTF8;

void gitwin_set_codepage(unsigned int codepage)
{
	_active_codepage = codepage;
}

unsigned int gitwin_get_codepage(void)
{
	return _active_codepage;
}

void gitwin_set_utf8(void)
{
	_active_codepage = CP_UTF8;
}

#define U16_LEAD(c) (wchar_t)(((c)>>10)+0xd7c0)
#define U16_TRAIL(c) (wchar_t)(((c)&0x3ff)|0xdc00)

void git__utf8_to_16(wchar_t *dest, const char *src, size_t srcLength)
{
	wchar_t *pDest = dest;
	uint32_t ch;
	const uint8_t* pSrc = (uint8_t*) src;
	const uint8_t *pSrcLimit = pSrc + srcLength;

	assert(dest && src && srcLength > 0);

	if ((pSrcLimit - pSrc) >= 4) {
		pSrcLimit -= 3; /* temporarily reduce pSrcLimit */

		/* in this loop, we can always access at least 4 bytes, up to pSrc+3 */
		do {
			ch = *pSrc++;
			if(ch < 0xc0) {
				/*
				 * ASCII, or a trail byte in lead position which is treated like
				 * a single-byte sequence for better character boundary
				 * resynchronization after illegal sequences.
				 */
				*pDest++=(wchar_t)ch;
			} else if(ch < 0xe0) { /* U+0080..U+07FF */
				/* 0x3080 = (0xc0 << 6) + 0x80 */
				*pDest++ = (wchar_t)((ch << 6) + *pSrc++ - 0x3080);
			} else if(ch < 0xf0) { /* U+0800..U+FFFF */
				/* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
				/* 0x2080 = (0x80 << 6) + 0x80 */
				ch = (ch << 12) + (*pSrc++ << 6);
				*pDest++ = (wchar_t)(ch + *pSrc++ - 0x2080);
			} else /* f0..f4 */ { /* U+10000..U+10FFFF */
				/* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
				ch = (ch << 18) + (*pSrc++ << 12);
				ch += *pSrc++ << 6;
				ch += *pSrc++ - 0x3c82080;
				*(pDest++) = U16_LEAD(ch);
				*(pDest++) = U16_TRAIL(ch);
			}
		} while(pSrc < pSrcLimit);

		pSrcLimit += 3; /* restore original pSrcLimit */
	}

	while(pSrc < pSrcLimit) {
		ch = *pSrc++;
		if(ch < 0xc0) {
			/*
			 * ASCII, or a trail byte in lead position which is treated like
			 * a single-byte sequence for better character boundary
			 * resynchronization after illegal sequences.
			 */
			*pDest++=(wchar_t)ch;
			continue;
		} else if(ch < 0xe0) { /* U+0080..U+07FF */
			if(pSrc < pSrcLimit) {
				/* 0x3080 = (0xc0 << 6) + 0x80 */
				*pDest++ = (wchar_t)((ch << 6) + *pSrc++ - 0x3080);
				continue;
			}
		} else if(ch < 0xf0) { /* U+0800..U+FFFF */
			if((pSrcLimit - pSrc) >= 2) {
				/* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
				/* 0x2080 = (0x80 << 6) + 0x80 */
				ch = (ch << 12) + (*pSrc++ << 6);
				*pDest++ = (wchar_t)(ch + *pSrc++ - 0x2080);
				pSrc += 3;
				continue;
			}
		} else /* f0..f4 */ { /* U+10000..U+10FFFF */
			if((pSrcLimit - pSrc) >= 3) {
				/* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
				ch = (ch << 18) + (*pSrc++ << 12);
				ch += *pSrc++ << 6;
				ch += *pSrc++ - 0x3c82080;
				*(pDest++) = U16_LEAD(ch);
				*(pDest++) = U16_TRAIL(ch);
				pSrc += 4;
				continue;
			}
		}

		/* truncated character at the end */
		*pDest++ = 0xfffd;
		break;
	}

	*pDest++ = 0x0;
}

wchar_t* gitwin_to_utf16(const char* str)
{
	wchar_t* ret;
	int cb;

	if (!str)
		return NULL;

	cb = MultiByteToWideChar(_active_codepage, 0, str, -1, NULL, 0);
	if (cb == 0)
		return (wchar_t *)git__calloc(1, sizeof(wchar_t));

	ret = (wchar_t *)git__malloc(cb * sizeof(wchar_t));
	if (!ret)
		return NULL;

	if (MultiByteToWideChar(_active_codepage, 0, str, -1, ret, (int)cb) == 0) {
		giterr_set(GITERR_OS, "Could not convert string to UTF-16");
		git__free(ret);
		ret = NULL;
	}

	return ret;
}

int gitwin_append_utf16(wchar_t *buffer, const char *str, size_t len)
{
	int result = MultiByteToWideChar(
		_active_codepage, 0, str, -1, buffer, (int)len);
	if (result == 0)
		giterr_set(GITERR_OS, "Could not convert string to UTF-16");
	return result;
}

char* gitwin_from_utf16(const wchar_t* str)
{
	char* ret;
	int cb;

	if (!str)
		return NULL;

	cb = WideCharToMultiByte(_active_codepage, 0, str, -1, NULL, 0, NULL, NULL);
	if (cb == 0)
		return (char *)git__calloc(1, sizeof(char));

	ret = (char*)git__malloc(cb);
	if (!ret)
		return NULL;

	if (WideCharToMultiByte(
		_active_codepage, 0, str, -1, ret, (int)cb, NULL, NULL) == 0)
	{
		giterr_set(GITERR_OS, "Could not convert string to UTF-8");
		git__free(ret);
		ret = NULL;
	}

	return ret;

}
