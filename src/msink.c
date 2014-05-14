/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "msink.h"

#ifndef NO_MMAP

static long s_mmap_pagesize = 0;

GIT_INLINE(long) mmap_pagesize(void)
{
	if (0 == s_mmap_pagesize)
		s_mmap_pagesize = git_mmap_pagesize();
	return s_mmap_pagesize;
}

GIT_INLINE(git_off_t) git_msink_curpos(git_msink_file *msf)
{
	return msf->pos.offset + msf->pos.cursor;
}

GIT_INLINE(git_off_t) git_msink_length(git_msink_file *msf)
{
	return max(msf->file_length, git_msink_curpos(msf));
}

GIT_INLINE(bool) git_msink_mapped(git_msink_file *msf)
{
	return NULL != msf->block.data;
}

bool git_msink_init(git_msink_file *msf, int fd)
{
	if (0 >= fd || git_msink_mapped(msf))
		return false;
	msf->fd = fd;
	msf->pos.offset = 0;
	msf->pos.cursor = 0;
	msf->file_length = 0;
	return true;
}

int git_msink_unmap(git_msink_file *msf)
{
	int error;
	if (!git_msink_mapped(msf))
		return 0;
	error = p_munmap(&msf->block);
	msf->block.data = NULL;
	return error;
}

int git_msink_write(git_msink_file *msf, const void *blob, size_t size)
{
	int error = 0;
	size_t wrsize;
	const char *data = (const char *) blob;

	for (; 0 != size; size -= wrsize)
	{
		if (NULL == msf->block.data) {
			error = p_mmap(&msf->block, mmap_pagesize()
				, GIT_PROT_WRITE, GIT_MAP_SHARED
				, msf->fd, msf->pos.offset);
			if (0 < error)
				break;
		}

		assert(msf->block.len > msf->pos.cursor);

		wrsize = min(size, msf->block.len - msf->pos.cursor);
		memcpy((char *) msf->block.data + msf->pos.cursor, data, wrsize);
		msf->pos.cursor += wrsize;
		data += wrsize;

		if (msf->block.len == msf->pos.cursor) {
			/* no more space left */
			error = git_msink_unmap(msf);
			if (0 > error)
				break;
			msf->pos.cursor = 0;
			msf->pos.offset += mmap_pagesize();
		}
	}

	return error;
}

GIT_INLINE(void) git_msink_get_pos(git_msink_pos *page, git_off_t offset)
{
	page->cursor = offset % mmap_pagesize();
	page->offset = offset - page->cursor;
}

git_off_t git_msink_seek(git_msink_file *msf, git_off_t offset, int whence)
{
	git_off_t curpos = git_msink_curpos(msf);

	switch (whence)
	{
	case SEEK_CUR:
		offset += curpos;
		break;

	case SEEK_END:
		offset += git_msink_length(msf);
		break;
	}

	if (0 <= offset) {
		git_off_t curpage = msf->pos.offset;
		/* if seeking back, make sure to remember the file length */
		if (curpos > offset && curpos > msf->file_length)
			msf->file_length = curpos;
		git_msink_get_pos(&msf->pos, offset);
		if (curpage != msf->pos.offset)
			git_msink_unmap(msf);
	}

	return offset;
}

bool git_msink_would_unmap(git_msink_file *msf, git_off_t offset)
{
	git_msink_pos page;

	if (!git_msink_mapped(msf))
		return false;

	git_msink_get_pos(&page, offset);
	return msf->pos.offset != page.offset;
}

bool git_msink_truncate(git_msink_file *msf)
{
	// might not be able to truncate if any mapped views are open
	return 0 <= git_msink_unmap(msf)
		&& 0 == p_ftruncate(msf->fd, git_msink_length(msf));
}

#endif
