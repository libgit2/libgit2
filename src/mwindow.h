/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_mwindow__
#define INCLUDE_mwindow__

#include "map.h"
#include "vector.h"

typedef struct git_mwindow {
	struct git_mwindow *next;
	git_map window_map;
	git_off_t offset;
	unsigned int last_used;
	unsigned int inuse_cnt;
} git_mwindow;

typedef struct git_mwindow_file {
	git_mwindow *windows;
	int fd;
	git_off_t size;
} git_mwindow_file;

typedef struct git_mwindow_ctl {
	size_t mapped;
	unsigned int open_windows;
	unsigned int mmap_calls;
	unsigned int peak_open_windows;
	size_t peak_mapped;
	size_t used_ctr;
	git_vector windowfiles;
} git_mwindow_ctl;

int git_mwindow_contains(git_mwindow *win, git_off_t offset);
void git_mwindow_free_all(git_mwindow_file *mwf);
unsigned char *git_mwindow_open(git_mwindow_file *mwf, git_mwindow **cursor, git_off_t offset, int extra, unsigned int *left);
void git_mwindow_scan_lru(git_mwindow_file *mwf, git_mwindow **lru_w, git_mwindow **lru_l);
int git_mwindow_file_register(git_mwindow_file *mwf);
void git_mwindow_close(git_mwindow **w_cursor);

#endif
