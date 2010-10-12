#include "test_lib.h"
#include "test_helpers.h"
#include "index.h"

#include <git/odb.h>
#include <git/index.h>

#define TEST_INDEX_PATH "../t0600-objects/index"

void print_entries(git_index *index)
{
	unsigned int i;

	for (i = 0; i < index->entry_count; ++i)
		printf("%d: %s\n", i, index->entries[i].path);
}

void randomize_entries(git_index *index)
{
	unsigned int i, j;
	git_index_entry tmp;

	srand((unsigned int)time(NULL));

	for (i = 0; i < index->entry_count; ++i) {
		j = rand() % index->entry_count;
		memcpy(&tmp, &index->entries[j], sizeof(git_index_entry));
		memcpy(&index->entries[j], &index->entries[i], sizeof(git_index_entry));
		memcpy(&index->entries[i], &tmp, sizeof(git_index_entry));
	}

	index->sorted = 0;
}

BEGIN_TEST(index_sort_test)
	git_index *index;
	unsigned int i;

	index = git_index_alloc(TEST_INDEX_PATH);
	must_be_true(index != NULL);
	must_pass(git_index_read(index));

	randomize_entries(index);

	git_index__sort(index);
	must_be_true(index->sorted);

	for (i = 1; i < index->entry_count; ++i)
		must_be_true(strcmp(index->entries[i - 1].path,
					index->entries[i].path) < 0);

	git_index_free(index);
END_TEST

BEGIN_TEST(index_sort_empty_test)
	git_index *index;
	index = git_index_alloc("fake-index");
	must_be_true(index != NULL);

	git_index__sort(index);
	must_be_true(index->sorted);

	git_index_free(index);
END_TEST
