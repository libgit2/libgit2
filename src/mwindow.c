/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "mwindow.h"
#include "vector.h"
#include "fileops.h"
#include "map.h"
#include "global.h"

#define DEFAULT_WINDOW_SIZE \
	(sizeof(void*) >= 8 \
		? 1 * 1024 * 1024 * 1024 \
		: 32 * 1024 * 1024)

#define DEFAULT_MAPPED_LIMIT \
	((1024 * 1024) * (sizeof(void*) >= 8 ? 8192ULL : 256UL))

/*
 * These are the global options for mmmap limits.
 * TODO: allow the user to change these
 */
static struct {
	size_t window_size;
	size_t mapped_limit;
} _mw_options = {
	DEFAULT_WINDOW_SIZE,
	DEFAULT_MAPPED_LIMIT,
};

/*
 * Free all the windows in a sequence, typically because we're done
 * with the file
 */
void git_mwindow_free_all(git_mwindow_file *mwf)
{
	git_mwindow_ctl *ctl = &GIT_GLOBAL->mem_ctl;
	unsigned int i;
	/*
	 * Remove these windows from the global list
	 */
	for (i = 0; i < ctl->windowfiles.length; ++i){
		if (git_vector_get(&ctl->windowfiles, i) == mwf) {
			git_vector_remove(&ctl->windowfiles, i);
			break;
		}
	}

	if (ctl->windowfiles.length == 0) {
		git_vector_free(&ctl->windowfiles);
		ctl->windowfiles.contents = NULL;
	}

	while (mwf->windows) {
		git_mwindow *w = mwf->windows;
		assert(w->inuse_cnt == 0);

		ctl->mapped -= w->window_map.len;
		ctl->open_windows--;

		git_futils_mmap_free(&w->window_map);

		mwf->windows = w->next;
		git__free(w);
	}
}

/*
 * Check if a window 'win' contains the address 'offset'
 */
int git_mwindow_contains(git_mwindow *win, git_off_t offset)
{
	git_off_t win_off = win->offset;
	return win_off <= offset
		&& offset <= (git_off_t)(win_off + win->window_map.len);
}

/*
 * Find the least-recently-used window in a file
 */
void git_mwindow_scan_lru(
	git_mwindow_file *mwf,
	git_mwindow **lru_w,
	git_mwindow **lru_l)
{
	git_mwindow *w, *w_l;

	for (w_l = NULL, w = mwf->windows; w; w = w->next) {
		if (!w->inuse_cnt) {
			/*
			 * If the current one is more recent than the last one,
			 * store it in the output parameter. If lru_w is NULL,
			 * it's the first loop, so store it as well.
			 */
			if (!*lru_w || w->last_used < (*lru_w)->last_used) {
				*lru_w = w;
				*lru_l = w_l;
			}
		}
		w_l = w;
	}
}

/*
 * Close the least recently used window. You should check to see if
 * the file descriptors need closing from time to time.
 */
static int git_mwindow_close_lru(git_mwindow_file *mwf)
{
	git_mwindow_ctl *ctl = &GIT_GLOBAL->mem_ctl;
	unsigned int i;
	git_mwindow *lru_w = NULL, *lru_l = NULL, **list = &mwf->windows;

	/* FIMXE: Does this give us any advantage? */
	if(mwf->windows)
		git_mwindow_scan_lru(mwf, &lru_w, &lru_l);

	for (i = 0; i < ctl->windowfiles.length; ++i) {
		git_mwindow *last = lru_w;
		git_mwindow_file *cur = git_vector_get(&ctl->windowfiles, i);
		git_mwindow_scan_lru(cur, &lru_w, &lru_l);
		if (lru_w != last)
			list = &cur->windows;
	}

	if (lru_w) {
		ctl->mapped -= lru_w->window_map.len;
		git_futils_mmap_free(&lru_w->window_map);

		if (lru_l)
			lru_l->next = lru_w->next;
		else
			*list = lru_w->next;

		git__free(lru_w);
		ctl->open_windows--;

		return GIT_SUCCESS;
	}

	return git__throw(GIT_ERROR, "Failed to close memory window. Couln't find LRU");
}

static git_mwindow *new_window(
	git_mwindow_file *mwf,
	git_file fd,
	git_off_t size,
	git_off_t offset)
{
	git_mwindow_ctl *ctl = &GIT_GLOBAL->mem_ctl;
	size_t walign = _mw_options.window_size / 2;
	git_off_t len;
	git_mwindow *w;

	w = git__malloc(sizeof(*w));
	if (w == NULL)
		return w;

	memset(w, 0x0, sizeof(*w));
	w->offset = (offset / walign) * walign;

	len = size - w->offset;
	if (len > (git_off_t)_mw_options.window_size)
		len = (git_off_t)_mw_options.window_size;

	ctl->mapped += (size_t)len;

	while (_mw_options.mapped_limit < ctl->mapped &&
			git_mwindow_close_lru(mwf) == GIT_SUCCESS) /* nop */;

	/*
	 * We treat _mw_options.mapped_limit as a soft limit. If we can't find a
	 * window to close and are above the limit, we still mmap the new
	 * window.
	 */

	if (git_futils_mmap_ro(&w->window_map, fd, w->offset, (size_t)len) < GIT_SUCCESS)
		goto cleanup;

	ctl->mmap_calls++;
	ctl->open_windows++;

	if (ctl->mapped > ctl->peak_mapped)
		ctl->peak_mapped = ctl->mapped;

	if (ctl->open_windows > ctl->peak_open_windows)
		ctl->peak_open_windows = ctl->open_windows;

	return w;

cleanup:
	git__free(w);
	return NULL;
}

/*
 * Open a new window, closing the least recenty used until we have
 * enough space. Don't forget to add it to your list
 */
unsigned char *git_mwindow_open(
	git_mwindow_file *mwf,
	git_mwindow **cursor,
	git_off_t offset,
	int extra,
	unsigned int *left)
{
	git_mwindow_ctl *ctl = &GIT_GLOBAL->mem_ctl;
	git_mwindow *w = *cursor;

	if (!w || !git_mwindow_contains(w, offset + extra)) {
		if (w) {
			w->inuse_cnt--;
		}

		for (w = mwf->windows; w; w = w->next) {
			if (git_mwindow_contains(w, offset + extra))
				break;
		}

		/*
		 * If there isn't a suitable window, we need to create a new
		 * one.
		 */
		if (!w) {
			w = new_window(mwf, mwf->fd, mwf->size, offset);
			if (w == NULL)
				return NULL;
			w->next = mwf->windows;
			mwf->windows = w;
		}
	}

	/* If we changed w, store it in the cursor */
	if (w != *cursor) {
		w->last_used = ctl->used_ctr++;
		w->inuse_cnt++;
		*cursor = w;
	}

	offset -= w->offset;
	assert(git__is_sizet(offset));

	if (left)
		*left = (unsigned int)(w->window_map.len - offset);

	return (unsigned char *) w->window_map.data + offset;
}

int git_mwindow_file_register(git_mwindow_file *mwf)
{
	git_mwindow_ctl *ctl = &GIT_GLOBAL->mem_ctl;
	int error;

	if (ctl->windowfiles.length == 0 &&
		(error = git_vector_init(&ctl->windowfiles, 8, NULL)) < GIT_SUCCESS)
		return error;

	return git_vector_insert(&ctl->windowfiles, mwf);
}

void git_mwindow_close(git_mwindow **window)
{
	git_mwindow *w = *window;
	if (w) {
		w->inuse_cnt--;
		*window = NULL;
	}
}
