#include "clar_libgit2.h"
#include "strmap.h"

GIT__USE_STRMAP;

void test_core_strmap__0(void)
{
	git_strmap *table = git_strmap_alloc();
	cl_assert(table != NULL);
	cl_assert(git_strmap_num_entries(table) == 0);
	git_strmap_free(table);
}

static void insert_strings(git_strmap *table, int count)
{
	int i, j, over, err;
	char *str;

	for (i = 0; i < count; ++i) {
		str = malloc(10);
		for (j = 0; j < 10; ++j)
			str[j] = 'a' + (i % 26);
		str[9] = '\0';

		/* if > 26, then encode larger value in first letters */
		for (j = 0, over = i / 26; over > 0; j++, over = over / 26)
			str[j] = 'A' + (over % 26);

		git_strmap_insert(table, str, str, err);
		cl_assert(err >= 0);
	}

	cl_assert((int)git_strmap_num_entries(table) == count);
}

void test_core_strmap__1(void)
{
	int i;
	char *str;
	git_strmap *table = git_strmap_alloc();
	cl_assert(table != NULL);

	insert_strings(table, 20);

	cl_assert(git_strmap_exists(table, "aaaaaaaaa"));
	cl_assert(git_strmap_exists(table, "ggggggggg"));
	cl_assert(!git_strmap_exists(table, "aaaaaaaab"));
	cl_assert(!git_strmap_exists(table, "abcdefghi"));

	i = 0;
	git_strmap_foreach_value(table, str, { i++; free(str); });
	cl_assert(i == 20);

	git_strmap_free(table);
}

void test_core_strmap__2(void)
{
	khiter_t pos;
	int i;
	char *str;
	git_strmap *table = git_strmap_alloc();
	cl_assert(table != NULL);

	insert_strings(table, 20);

	cl_assert(git_strmap_exists(table, "aaaaaaaaa"));
	cl_assert(git_strmap_exists(table, "ggggggggg"));
	cl_assert(!git_strmap_exists(table, "aaaaaaaab"));
	cl_assert(!git_strmap_exists(table, "abcdefghi"));

	cl_assert(git_strmap_exists(table, "bbbbbbbbb"));
	pos = git_strmap_lookup_index(table, "bbbbbbbbb");
	cl_assert(git_strmap_valid_index(table, pos));
	cl_assert_equal_s(git_strmap_value_at(table, pos), "bbbbbbbbb");
	free(git_strmap_value_at(table, pos));
	git_strmap_delete_at(table, pos);

	cl_assert(!git_strmap_exists(table, "bbbbbbbbb"));

	i = 0;
	git_strmap_foreach_value(table, str, { i++; free(str); });
	cl_assert(i == 19);

	git_strmap_free(table);
}

void test_core_strmap__3(void)
{
	int i;
	char *str;
	git_strmap *table = git_strmap_alloc();
	cl_assert(table != NULL);

	insert_strings(table, 10000);

	i = 0;
	git_strmap_foreach_value(table, str, { i++; free(str); });
	cl_assert(i == 10000);

	git_strmap_free(table);
}
