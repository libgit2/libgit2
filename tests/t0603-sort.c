#include "test_lib.h"
#include "test_helpers.h"
#include "index.h"

#include <git2/odb.h>
#include <git2/index.h>

/*
void print_entries(git_index *index)
{
	unsigned int i;

	for (i = 0; i < index->entries.length; ++i)
		printf("%d: %s\n", i, index->entries[i].path);
}
*/

void randomize_entries(git_index *index)
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

BEGIN_TEST(index_sort_test)
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

BEGIN_TEST(index_sort_empty_test)
	git_index *index;

	must_pass(git_index_open_bare(&index, "fake-index"));

	git_index__sort(index);
	must_be_true(index->sorted);

	git_index_free(index);
END_TEST
