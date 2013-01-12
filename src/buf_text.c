/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "buf_text.h"
#include "fileops.h"

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

#define SIMILARITY_MAXRUN 256
#define SIMILARITY_HASH_START  5381
#define SIMILARITY_HASH_UPDATE(S,N) (((S) << 5) + (S) + (uint32_t)(N))

enum {
	SIMILARITY_FORMAT_UNKNOWN = 0,
	SIMILARITY_FORMAT_TEXT = 1,
	SIMILARITY_FORMAT_BINARY = 2
};

struct git_buf_text_hashsig {
	uint32_t *hashes;
	size_t size;
	size_t asize;
	unsigned int format : 2;
	unsigned int pairs : 1;
};

static int similarity_advance(git_buf_text_hashsig *sig, uint32_t hash)
{
	if (sig->size >= sig->asize) {
		size_t new_asize = sig->asize + 512;
		uint32_t *new_hashes =
			git__realloc(sig->hashes, new_asize * sizeof(uint32_t));
		GITERR_CHECK_ALLOC(new_hashes);

		sig->hashes = new_hashes;
		sig->asize  = new_asize;
	}

	sig->hashes[sig->size++] = hash;
	return 0;
}

static int similarity_add_hashes(
	git_buf_text_hashsig *sig,
	uint32_t *hash_start,
	size_t *hashlen_start,
	const char *ptr,
	size_t len)
{
	int error = 0;
	const char *scan = ptr, *scan_end = ptr + len;
	char term = (sig->format == SIMILARITY_FORMAT_TEXT) ? '\n' : '\0';
	uint32_t hash = hash_start ? *hash_start : SIMILARITY_HASH_START;
	size_t hashlen = hashlen_start ? *hashlen_start : 0;

	while (scan < scan_end) {
		char ch = *scan++;

		if (ch == term || hashlen >= SIMILARITY_MAXRUN) {
			if ((error = similarity_advance(sig, hash)) < 0)
				break;

			hash = SIMILARITY_HASH_START;
			hashlen = 0;

			/* skip run of terminators */
			while (scan < scan_end && *scan == term)
				scan++;
		} else {
			hash = SIMILARITY_HASH_UPDATE(hash, ch);
			hashlen++;
		}
	}

	if (hash_start)
		*hash_start = hash;
	if (hashlen_start)
		*hashlen_start = hashlen;

	/* if we're not saving intermediate state, add final hash as needed */
	if (!error && !hash_start && hashlen > 0)
		error = similarity_advance(sig, hash);

	return error;
}

/*
 * Decide if \0 or \n terminated runs are a better choice for hashes
 */
static void similarity_guess_format(
	git_buf_text_hashsig *sig, const char *ptr, size_t len)
{
	size_t lines = 0, line_length = 0, max_line_length = 0;
	size_t runs = 0, run_length = 0, max_run_length = 0;

	/* don't process more than 4k of data for this */
	if (len > 4096)
		len = 4096;

	/* gather some stats */
	while (len--) {
		char ch = *ptr++;

		if (ch == '\0') {
			runs++;
			if (max_run_length < run_length)
				max_run_length = run_length;
			run_length = 0;
		} else if (ch == '\n') {
			lines++;
			if (max_line_length < line_length)
				max_line_length = line_length;
			line_length = 0;
		} else {
			run_length++;
			line_length++;
		}
	}

	/* the following heuristic could probably be improved */
	if (lines > runs)
		sig->format = SIMILARITY_FORMAT_TEXT;
	else if (runs > 0)
		sig->format = SIMILARITY_FORMAT_BINARY;
	else
		sig->format = SIMILARITY_FORMAT_UNKNOWN;
}

static int similarity_compare_score(const void *a, const void *b)
{
	uint32_t av = *(uint32_t *)a, bv = *(uint32_t *)b;
	return (av < bv) ? -1 : (av > bv) ? 1 : 0;
}

static int similarity_finalize_hashes(
	git_buf_text_hashsig *sig, bool generate_pairs)
{
	if (!sig->size)
		return 0;

	/* create pairwise hashes if requested */

	if (generate_pairs) {
		size_t i, needed_size = sig->size * 2 - 1;

		if (needed_size > sig->asize) {
			uint32_t *new_hashes =
				git__realloc(sig->hashes, needed_size * sizeof(uint32_t));
			GITERR_CHECK_ALLOC(new_hashes);

			sig->hashes = new_hashes;
			sig->asize  = needed_size;
		}

		for (i = 1; i < sig->size; ++i)
			sig->hashes[sig->size + i - 1] =
				SIMILARITY_HASH_UPDATE(sig->hashes[i - 1], sig->hashes[i]);

		sig->pairs = 1;
	}

	/* sort all hashes */

	qsort(sig->hashes, sig->size, sizeof(uint32_t), similarity_compare_score);

	if (generate_pairs)
		qsort(&sig->hashes[sig->size], sig->size - 1, sizeof(uint32_t),
			similarity_compare_score);

	return 0;
}

int git_buf_text_hashsig_create(
	git_buf_text_hashsig **out,
	const git_buf *buf,
	bool generate_pairs)
{
	int error;
	git_buf_text_hashsig *sig = git__calloc(1, sizeof(git_buf_text_hashsig));
	GITERR_CHECK_ALLOC(sig);

	similarity_guess_format(sig, buf->ptr, buf->size);

	error = similarity_add_hashes(sig, NULL, NULL, buf->ptr, buf->size);

	if (!error)
		error = similarity_finalize_hashes(sig, generate_pairs);

	if (!error)
		*out = sig;
	else
		git_buf_text_hashsig_free(sig);

	return error;
}

int git_buf_text_hashsig_create_fromfile(
	git_buf_text_hashsig **out,
	const char *path,
	bool generate_pairs)
{
	char buf[4096];
	ssize_t buflen = 0;
	uint32_t hash = SIMILARITY_HASH_START;
	size_t hashlen = 0;
	int error = 0, fd;
	git_buf_text_hashsig *sig = git__calloc(1, sizeof(git_buf_text_hashsig));
	GITERR_CHECK_ALLOC(sig);

	if ((fd = git_futils_open_ro(path)) < 0) {
		git__free(sig);
		return fd;
	}

	while (!error && (buflen = p_read(fd, buf, sizeof(buf))) > 0) {
		if (sig->format == SIMILARITY_FORMAT_UNKNOWN)
			similarity_guess_format(sig, buf, buflen);

		error = similarity_add_hashes(sig, &hash, &hashlen, buf, buflen);
	}

	if (buflen < 0) {
		giterr_set(GITERR_OS,
			"Read error on '%s' while calculating similarity hashes", path);
		error = (int)buflen;
	}

	p_close(fd);

	if (!error && hashlen > 0)
		error = similarity_advance(sig, hash);

	if (!error)
		error = similarity_finalize_hashes(sig, generate_pairs);

	if (!error)
		*out = sig;
	else
		git_buf_text_hashsig_free(sig);

	return error;
}

void git_buf_text_hashsig_free(git_buf_text_hashsig *sig)
{
	if (!sig)
		return;

	if (sig->hashes) {
		git__free(sig->hashes);
		sig->hashes = NULL;
	}

	git__free(sig);
}

int git_buf_text_hashsig_compare(
	const git_buf_text_hashsig *a,
	const git_buf_text_hashsig *b,
	int scale)
{
	size_t matches = 0, pairs = 0, total = 0, i, j;

	if (a->format != b->format || !a->size || !b->size)
		return 0;

	if (scale <= 0)
		scale = 100;

	/* hash lists are sorted - just look for overlap vs total */

	for (i = 0, j = 0; i < a->size && j < b->size; ) {
		uint32_t av = a->hashes[i];
		uint32_t bv = b->hashes[j];

		if (av < bv)
			++i;
		else if (av > bv)
			++j;
		else {
			++i; ++j;
			++matches;
		}
	}

	total = (a->size + b->size);

	if (a->pairs && b->pairs) {
		for (i = 0, j = 0; i < a->size - 1 && j < b->size - 1; ) {
			uint32_t av = a->hashes[i + a->size];
			uint32_t bv = b->hashes[j + b->size];

			if (av < bv)
				++i;
			else if (av > bv)
				++j;
			else {
				++i; ++j;
				++pairs;
			}
		}

		total += (a->size + b->size - 2);
	}

	return (int)(scale * 2 * (matches + pairs) / total);
}

