/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "buf_text.h"

int git_buf_text_puts_escaped(
	git_buf *buf,
	const char *string,
	const char *esc_chars,
	const char *esc_with)
{
	const char *scan;
	size_t total = 0, esc_len = strlen(esc_with), count;

	if (!string)
		return 0;

	for (scan = string; *scan; ) {
		/* count run of non-escaped characters */
		count = strcspn(scan, esc_chars);
		total += count;
		scan += count;
		/* count run of escaped characters */
		count = strspn(scan, esc_chars);
		total += count * (esc_len + 1);
		scan += count;
	}

	if (git_buf_grow(buf, buf->size + total + 1) < 0)
		return -1;

	for (scan = string; *scan; ) {
		count = strcspn(scan, esc_chars);

		memmove(buf->ptr + buf->size, scan, count);
		scan += count;
		buf->size += count;

		for (count = strspn(scan, esc_chars); count > 0; --count) {
			/* copy escape sequence */
			memmove(buf->ptr + buf->size, esc_with, esc_len);
			buf->size += esc_len;
			/* copy character to be escaped */
			buf->ptr[buf->size] = *scan;
			buf->size++;
			scan++;
		}
	}

	buf->ptr[buf->size] = '\0';

	return 0;
}

void git_buf_text_unescape(git_buf *buf)
{
	buf->size = git__unescape(buf->ptr);
}

int git_buf_text_common_prefix(git_buf *buf, const git_strarray *strings)
{
	size_t i;
	const char *str, *pfx;

	git_buf_clear(buf);

	if (!strings || !strings->count)
		return 0;

	/* initialize common prefix to first string */
	if (git_buf_sets(buf, strings->strings[0]) < 0)
		return -1;

	/* go through the rest of the strings, truncating to shared prefix */
	for (i = 1; i < strings->count; ++i) {

		for (str = strings->strings[i], pfx = buf->ptr;
			 *str && *str == *pfx; str++, pfx++)
			/* scanning */;

		git_buf_truncate(buf, pfx - buf->ptr);

		if (!buf->size)
			break;
	}

	return 0;
}

bool git_buf_text_is_binary(const git_buf *buf)
{
	const char *scan = buf->ptr, *end = buf->ptr + buf->size;
	int printable = 0, nonprintable = 0;

	while (scan < end) {
		unsigned char c = *scan++;

		if (c > 0x1F && c < 0x7F)
			printable++;
		else if (c == '\0')
			return true;
		else if (!git__isspace(c))
			nonprintable++;
	}

	return ((printable >> 7) < nonprintable);
}

bool git_buf_text_contains_nul(const git_buf *buf)
{
	return (memchr(buf->ptr, '\0', buf->size) != NULL);
}

int git_buf_text_detect_bom(git_bom_t *bom, const git_buf *buf, size_t offset)
{
	const char *ptr;
	size_t len;

	*bom = GIT_BOM_NONE;
	/* need at least 2 bytes after offset to look for any BOM */
	if (buf->size < offset + 2)
		return 0;

	ptr = buf->ptr + offset;
	len = buf->size - offset;

	switch (*ptr++) {
	case 0:
		if (len >= 4 && ptr[0] == 0 && ptr[1] == '\xFE' && ptr[2] == '\xFF') {
			*bom = GIT_BOM_UTF32_BE;
			return 4;
		}
		break;
	case '\xEF':
		if (len >= 3 && ptr[0] == '\xBB' && ptr[1] == '\xBF') {
			*bom = GIT_BOM_UTF8;
			return 3;
		}
		break;
	case '\xFE':
		if (*ptr == '\xFF') {
			*bom = GIT_BOM_UTF16_BE;
			return 2;
		}
		break;
	case '\xFF':
		if (*ptr != '\xFE')
			break;
		if (len >= 4 && ptr[1] == 0 && ptr[2] == 0) {
			*bom = GIT_BOM_UTF32_LE;
			return 4;
		} else {
			*bom = GIT_BOM_UTF16_LE;
			return 2;
		}
		break;
	default:
		break;
	}

	return 0;
}

bool git_buf_text_gather_stats(
	git_buf_text_stats *stats, const git_buf *buf, bool skip_bom)
{
	const char *scan = buf->ptr, *end = buf->ptr + buf->size;
	int skip;

	memset(stats, 0, sizeof(*stats));

	/* BOM detection */
	skip = git_buf_text_detect_bom(&stats->bom, buf, 0);
	if (skip_bom)
		scan += skip;

	/* Ignore EOF character */
	if (buf->size > 0 && end[-1] == '\032')
		end--;

	/* Counting loop */
	while (scan < end) {
		unsigned char c = *scan++;

		if ((c > 0x1F && c < 0x7F) || c > 0x9f)
			stats->printable++;
		else switch (c) {
			case '\0':
				stats->nul++;
				stats->nonprintable++;
				break;
			case '\n':
				stats->lf++;
				break;
			case '\r':
				stats->cr++;
				if (scan < end && *scan == '\n')
					stats->crlf++;
				break;
			case '\t': case '\f': case '\v': case '\b': case 0x1b: /*ESC*/
				stats->printable++;
				break;
			default:
				stats->nonprintable++;
				break;
			}
	}

	return (stats->nul > 0 ||
		((stats->printable >> 7) < stats->nonprintable));
}
