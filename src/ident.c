/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/sys/filter.h"
#include "filter.h"
#include "buffer.h"

int ident_find_id(
	const char **id_start, const char **id_end, const char *start, size_t len)
{
	const char *found;

	while (len > 0 && (found = memchr(start, '$', len)) != NULL) {
		size_t remaining = len - (size_t)(found - start);
		if (remaining < 3)
			return GIT_ENOTFOUND;
		if (found[1] == 'I' && found[2] == 'd')
			break;
		start = found + 1;
		len = remaining - 1;
	}

	if (len < 3)
		return GIT_ENOTFOUND;
	*id_start = found;

	if ((found = memchr(found + 3, '$', len - 3)) == NULL)
		return GIT_ENOTFOUND;

	*id_end = found + 1;
	return 0;
}

static int ident_insert_id(
	git_buffer *to, const git_buffer *from, const git_filter_source *src)
{
	char oid[GIT_OID_HEXSZ+1];
	const char *id_start, *id_end, *from_end = from->ptr + from->size;
	size_t need_size;
	git_buf to_buf = GIT_BUF_FROM_BUFFER(to);

	/* replace $Id$ with blob id */

	if (!git_filter_source_id(src))
		return GIT_ENOTFOUND;

	git_oid_tostr(oid, sizeof(oid), git_filter_source_id(src));

	if (ident_find_id(&id_start, &id_end, from->ptr, from->size) < 0)
		return GIT_ENOTFOUND;

	need_size = (size_t)(id_start - from->ptr) +
		5 /* "$Id: " */ + GIT_OID_HEXSZ + 1 /* "$" */ +
		(size_t)(from_end - id_end);

	if (git_buf_grow(&to_buf, need_size) < 0)
		return -1;

	git_buf_set(&to_buf, from->ptr, (size_t)(id_start - from->ptr));
	git_buf_put(&to_buf, "$Id: ", 5);
	git_buf_put(&to_buf, oid, GIT_OID_HEXSZ);
	git_buf_putc(&to_buf, '$');
	git_buf_put(&to_buf, id_end, (size_t)(from_end - id_end));

	if (git_buf_oom(&to_buf))
		return -1;

	git_buffer_from_buf(to, &to_buf);
	return 0;
}

static int ident_remove_id(
	git_buffer *to, const git_buffer *from)
{
	const char *id_start, *id_end, *from_end = from->ptr + from->size;
	size_t need_size;
	git_buf to_buf = GIT_BUF_FROM_BUFFER(to);

	if (ident_find_id(&id_start, &id_end, from->ptr, from->size) < 0)
		return GIT_ENOTFOUND;

	need_size = (size_t)(id_start - from->ptr) +
		4 /* "$Id$" */ + (size_t)(from_end - id_end);

	if (git_buf_grow(&to_buf, need_size) < 0)
		return -1;

	git_buf_set(&to_buf, from->ptr, (size_t)(id_start - from->ptr));
	git_buf_put(&to_buf, "$Id$", 4);
	git_buf_put(&to_buf, id_end, (size_t)(from_end - id_end));

	if (git_buf_oom(&to_buf))
		return -1;

	git_buffer_from_buf(to, &to_buf);
	return 0;
}

static int ident_apply(
	git_filter        *self,
	void              **payload,
	git_buffer        *to,
	const git_buffer  *from,
	const git_filter_source *src)
{
	GIT_UNUSED(self); GIT_UNUSED(payload);

	if (git_filter_source_mode(src) == GIT_FILTER_SMUDGE)
		return ident_insert_id(to, from, src);
	else
		return ident_remove_id(to, from);
}

git_filter *git_ident_filter_new(void)
{
	git_filter *f = git__calloc(1, sizeof(git_filter));

	f->version = GIT_FILTER_VERSION;
	f->attributes = "+ident"; /* apply to files with ident attribute set */
	f->shutdown = git_filter_free;
	f->apply    = ident_apply;

	return f;
}
