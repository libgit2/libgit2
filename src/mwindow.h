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

#ifndef INCLUDE_mwindow__
#define INCLUDE_mwindow__

#include "map.h"
#include "vector.h"
#include "fileops.h"

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
	size_t window_size; /* needs default value */
	size_t mapped_limit; /* needs default value */
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
