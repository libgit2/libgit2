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
	time_t mtime;
};

struct test_entry TEST_ENTRIES[] = {
	{4, "Makefile", 5064, 0x4C3F7F33},
	{62, "tests/Makefile", 2631, 0x4C3F7F33},
	{36, "src/index.c", 10014, 0x4C43368D},
	{6, "git.git-authors", 2709, 0x4C3F7F33},
	{48, "src/revobject.h", 1448, 0x4C3F7FE2}
};

BEGIN_TEST("read", index_loadempty_test)
	git_index *index;

	must_pass(git_index_open_bare(&index, "in-memory-index"));
	must_be_true(index->on_disk == 0);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk == 0);
	must_be_true(git_index_entrycount(index) == 0);
	must_be_true(index->sorted);

	git_index_free(index);
END_TEST

BEGIN_TEST("read", index_load_test)
	git_index *index;
	unsigned int i;
	git_index_entry **entries;

	must_pass(git_index_open_bare(&index, TEST_INDEX_PATH));
	must_be_true(index->on_disk);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk);
	must_be_true(git_index_entrycount(index) == TEST_INDEX_ENTRY_COUNT);
	must_be_true(index->sorted);

	entries = (git_index_entry **)index->entries.contents;

	for (i = 0; i < ARRAY_SIZE(TEST_ENTRIES); ++i) {
		git_index_entry *e = entries[TEST_ENTRIES[i].index];

		must_be_true(strcmp(e->path, TEST_ENTRIES[i].path) == 0);
		must_be_true(e->mtime.seconds == TEST_ENTRIES[i].mtime);
		must_be_true(e->file_size == TEST_ENTRIES[i].file_size);
	}

	git_index_free(index);
END_TEST

BEGIN_TEST("read", index2_load_test)
	git_index *index;

	must_pass(git_index_open_bare(&index, TEST_INDEX2_PATH));
	must_be_true(index->on_disk);

	must_pass(git_index_read(index));

	must_be_true(index->on_disk);
	must_be_true(git_index_entrycount(index) == TEST_INDEX2_ENTRY_COUNT);
	must_be_true(index->sorted);
	must_be_true(index->tree != NULL);

	git_index_free(index);
END_TEST

BEGIN_TEST("read", index_find_test)
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

BEGIN_TEST("read", index_findempty_test)
	git_index *index;
	unsigned int i;

	must_pass(git_index_open_bare(&index, "fake-index"));

	for (i = 0; i < ARRAY_SIZE(TEST_ENTRIES); ++i) {
		int idx = git_index_find(index, TEST_ENTRIES[i].path);
		must_be_true(idx == GIT_ENOTFOUND);
	}

	git_index_free(index);
END_TEST

BEGIN_TEST("write", index_write_test)
	git_index *index;
	git_filelock out_file;

	must_pass(git_index_open_bare(&index, TEST_INDEX_PATH));
	must_pass(git_index_read(index));
	must_be_true(index->on_disk);

	must_pass(git_filelock_init(&out_file, "index_rewrite"));
	must_pass(git_filelock_lock(&out_file, 0));
	must_pass(git_index__write(index, &out_file));
	must_pass(git_filelock_commit(&out_file));

	git_index_free(index);
	
	gitfo_unlink("index_rewrite");
END_TEST


static void randomize_entries(git_index *index)
{
	unsigned int i, j;
	git_index_entry *tmp;
	git_index_entry **entries;

	entries = (git_index_entry **)index->entries.contents;

	srand((unsigned int)time(NULL));

	for (i = 0; i < index->entries.length; ++i) {
		j = rand() % index->entries.length;

		tmp = entries[j];
		entries[j] = entries[i];
		entries[i] = tmp;
	}

	index->sorted = 0;
}

BEGIN_TEST("sort", index_sort_test)
	git_index *index;
	unsigned int i;
	git_index_entry **entries;

	must_pass(git_index_open_bare(&index, TEST_INDEX_PATH));
	must_pass(git_index_read(index));

	randomize_entries(index);

	git_index__sort(index);
	must_be_true(index->sorted);

	entries = (git_index_entry **)index->entries.contents;

	for (i = 1; i < index->entries.length; ++i)
		must_be_true(strcmp(entries[i - 1]->path, entries[i]->path) < 0);

	git_index_free(index);
END_TEST

BEGIN_TEST("sort", index_sort_empty_test)
	git_index *index;

	must_pass(git_index_open_bare(&index, "fake-index"));

	git_index__sort(index);
	must_be_true(index->sorted);

	git_index_free(index);
END_TEST


git_testsuite *libgit2_suite_index(void)
{
	git_testsuite *suite = git_testsuite_new("Index");

	ADD_TEST(suite, "read", index_loadempty_test);
	ADD_TEST(suite, "read", index_load_test);
	ADD_TEST(suite, "read", index2_load_test);
	ADD_TEST(suite, "read", index_find_test);
	ADD_TEST(suite, "read", index_findempty_test);
	ADD_TEST(suite, "write", index_write_test);
	ADD_TEST(suite, "sort", index_sort_test);
	ADD_TEST(suite, "sort", index_sort_empty_test);

	return suite;
}
