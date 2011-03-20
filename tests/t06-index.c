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
#include "test_lib.h"
#include "test_helpers.h"

#include "index.h"

#define TEST_INDEX_ENTRY_COUNT 109
#define TEST_INDEX2_ENTRY_COUNT 1437

struct test_entry {
	unsigned int index;
	char path[128];
	git_off_t file_size;
	git_time_t mtime;
};

struct test_entry TEST_ENTRIES[] = {
	{4, "Makefile", 5064, 0x4C3F7F33},
	{62, "tests/Makefile", 2631, 0x4C3F7F33},
	{36, "src/index.c", 10014, 0x4C43368D},
	{6, "git.git-authors", 2709, 0x4C3F7F33},
	{48, "src/revobject.h", 1448, 0x4C3F7FE2}
};

BEGIN_TEST(read0, "load an empty index")
	git_index *index;

	must_pass(git_index_open_bare(&index, "in-memory-index"));
	must_be_true(index->on_disk == 0);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk == 0);
	must_be_true(git_index_entrycount(index) == 0);
	must_be_true(index->entries.sorted);

	git_index_free(index);
END_TEST

BEGIN_TEST(read1, "load a standard index (default test index)")
	git_index *index;
	unsigned int i;
	git_index_entry **entries;

	must_pass(git_index_open_bare(&index, TEST_INDEX_PATH));
	must_be_true(index->on_disk);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk);
	must_be_true(git_index_entrycount(index) == TEST_INDEX_ENTRY_COUNT);
	must_be_true(index->entries.sorted);

	entries = (git_index_entry **)index->entries.contents;

	for (i = 0; i < ARRAY_SIZE(TEST_ENTRIES); ++i) {
		git_index_entry *e = entries[TEST_ENTRIES[i].index];

		must_be_true(strcmp(e->path, TEST_ENTRIES[i].path) == 0);
		must_be_true(e->mtime.seconds == TEST_ENTRIES[i].mtime);
		must_be_true(e->file_size == TEST_ENTRIES[i].file_size);
	}

	git_index_free(index);
END_TEST

BEGIN_TEST(read2, "load a standard index (git.git index)")
	git_index *index;

	must_pass(git_index_open_bare(&index, TEST_INDEX2_PATH));
	must_be_true(index->on_disk);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk);
	must_be_true(git_index_entrycount(index) == TEST_INDEX2_ENTRY_COUNT);
	must_be_true(index->entries.sorted);
	must_be_true(index->tree != NULL);

	git_index_free(index);
END_TEST

BEGIN_TEST(find0, "find an entry on an index")
	git_index *index;
	unsigned int i;

	must_pass(git_index_open_bare(&index, TEST_INDEX_PATH));
	must_pass(git_index_read(index));

	for (i = 0; i < ARRAY_SIZE(TEST_ENTRIES); ++i) {
		int idx = git_index_find(index, TEST_ENTRIES[i].path);
		must_be_true((unsigned int)idx == TEST_ENTRIES[i].index);
	}

	git_index_free(index);
END_TEST

BEGIN_TEST(find1, "find an entry in an empty index")
	git_index *index;
	unsigned int i;

	must_pass(git_index_open_bare(&index, "fake-index"));

	for (i = 0; i < ARRAY_SIZE(TEST_ENTRIES); ++i) {
		int idx = git_index_find(index, TEST_ENTRIES[i].path);
		must_be_true(idx == GIT_ENOTFOUND);
	}

	git_index_free(index);
END_TEST

BEGIN_TEST(write0, "write an index back to disk")
	git_index *index;

	must_pass(copy_file(TEST_INDEXBIG_PATH, "index_rewrite"));

	must_pass(git_index_open_bare(&index, "index_rewrite"));
	must_pass(git_index_read(index));
	must_be_true(index->on_disk);

	must_pass(git_index_write(index));
	must_pass(cmp_files(TEST_INDEXBIG_PATH, "index_rewrite"));

	git_index_free(index);
	
	gitfo_unlink("index_rewrite");
END_TEST

BEGIN_TEST(sort0, "sort the entires in an index")
	/*
	 * TODO: This no longer applies:
	 * index sorting in Git uses some specific changes to the way
	 * directories are sorted.
	 *
	 * We need to specificially check for this by creating a new
	 * index, adding entries in random order and then
	 * checking for consistency
	 */
END_TEST


BEGIN_TEST(sort1, "sort the entires in an empty index")
	git_index *index;

	must_pass(git_index_open_bare(&index, "fake-index"));

	/* FIXME: this test is slightly dumb */
	must_be_true(index->entries.sorted);

	git_index_free(index);
END_TEST

BEGIN_SUITE(index)
	ADD_TEST(read0);
	ADD_TEST(read1);
	ADD_TEST(read2);

	ADD_TEST(find0);
	ADD_TEST(find1);

	ADD_TEST(write0);

	ADD_TEST(sort0);
	ADD_TEST(sort1);
END_SUITE
