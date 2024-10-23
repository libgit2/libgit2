#include "clar_libgit2.h"
#include "hashmap.h"
#include "hashmap_str.h"

GIT_HASHMAP_STR_SETUP(git_hashmap_test, char *);

static git_hashmap_test g_table;

void test_hashmap__initialize(void)
{
	memset(&g_table, 0x0, sizeof(git_hashmap_test));
}

void test_hashmap__cleanup(void)
{
	git_hashmap_test_dispose(&g_table);
}

void test_hashmap__0(void)
{
	cl_assert(git_hashmap_test_size(&g_table) == 0);
}

static void insert_strings(git_hashmap_test *table, size_t count)
{
	size_t i, j, over;
	char *str;

	for (i = 0; i < count; ++i) {
		str = git__malloc(10);
		for (j = 0; j < 10; ++j)
			str[j] = 'a' + (i % 26);
		str[9] = '\0';

		/* if > 26, then encode larger value in first letters */
		for (j = 0, over = i / 26; over > 0; j++, over = over / 26)
			str[j] = 'A' + (over % 26);

		cl_git_pass(git_hashmap_test_put(table, str, str));
	}

	cl_assert_equal_i(git_hashmap_test_size(table), count);
}

void test_hashmap__inserted_strings_can_be_retrieved(void)
{
	char *str;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;
	size_t idx = 0;

	insert_strings(&g_table, 20);

	cl_assert(git_hashmap_test_contains(&g_table, "aaaaaaaaa"));
	cl_assert(git_hashmap_test_contains(&g_table, "ggggggggg"));
	cl_assert(!git_hashmap_test_contains(&g_table, "aaaaaaaab"));
	cl_assert(!git_hashmap_test_contains(&g_table, "abcdefghi"));

	while (git_hashmap_test_iterate(&iter, NULL, &str, &g_table) == 0) {
		idx++;
		git__free(str);
	}

	cl_assert_equal_sz(20, idx);
}

void test_hashmap__deleted_entry_cannot_be_retrieved(void)
{
	const char *key;
	char *str;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;
	size_t idx = 0;

	insert_strings(&g_table, 20);

	cl_assert(git_hashmap_test_contains(&g_table, "bbbbbbbbb"));
	cl_git_pass(git_hashmap_test_get(&str, &g_table, "bbbbbbbbb"));
	cl_assert_equal_s(str, "bbbbbbbbb");
	cl_git_pass(git_hashmap_test_remove(&g_table, "bbbbbbbbb"));
	git__free(str);

	cl_assert(!git_hashmap_test_contains(&g_table, "bbbbbbbbb"));

	while (git_hashmap_test_iterate(&iter, &key, &str, &g_table) == 0) {
		idx++;
		git__free(str);
	}

	cl_assert_equal_sz(idx, 19);
}

void test_hashmap__inserting_many_keys_succeeds(void)
{
	char *str;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;
	size_t idx = 0;

	insert_strings(&g_table, 10000);

	while (git_hashmap_test_iterate(&iter, NULL, &str, &g_table) == 0) {
		idx++;
		git__free(str);
	}

	cl_assert_equal_sz(idx, 10000);
}

void test_hashmap__get_succeeds_with_existing_entries(void)
{
	const char *keys[] = { "foo", "bar", "gobble" };
	char *values[] = { "oof", "rab", "elbbog" };
	char *str;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(keys); i++)
		cl_git_pass(git_hashmap_test_put(&g_table, keys[i], values[i]));

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "foo"));
	cl_assert_equal_s(str, "oof");

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "bar"));
	cl_assert_equal_s(str, "rab");

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "gobble"));
	cl_assert_equal_s(str, "elbbog");
}

void test_hashmap__get_returns_notfound_on_nonexisting_key(void)
{
	const char *keys[] = { "foo", "bar", "gobble" };
	char *values[] = { "oof", "rab", "elbbog" };
	char *str;
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(keys); i++)
		cl_git_pass(git_hashmap_test_put(&g_table, keys[i], values[i]));

	cl_git_fail_with(GIT_ENOTFOUND, git_hashmap_test_get(&str, &g_table, "other"));
}

void test_hashmap__put_persists_key(void)
{
	char *str;

	cl_git_pass(git_hashmap_test_put(&g_table, "foo", "oof"));

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "foo"));
	cl_assert_equal_s(str, "oof");
}

void test_hashmap__put_persists_multpile_keys(void)
{
	char *str;

	cl_git_pass(git_hashmap_test_put(&g_table, "foo", "oof"));
	cl_git_pass(git_hashmap_test_put(&g_table, "bar", "rab"));

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "foo"));
	cl_assert_equal_s(str, "oof");

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "bar"));
	cl_assert_equal_s(str, "rab");
}

void test_hashmap__put_updates_existing_key(void)
{
	char *str;

	cl_git_pass(git_hashmap_test_put(&g_table, "foo", "oof"));
	cl_git_pass(git_hashmap_test_put(&g_table, "bar", "rab"));
	cl_git_pass(git_hashmap_test_put(&g_table, "gobble", "elbbog"));
	cl_assert_equal_i(3, git_hashmap_test_size(&g_table));

	cl_git_pass(git_hashmap_test_put(&g_table, "foo", "other"));
	cl_assert_equal_i(git_hashmap_test_size(&g_table), 3);

	cl_git_pass(git_hashmap_test_get(&str, &g_table, "foo"));
	cl_assert_equal_s(str, "other");
}

void test_hashmap__iteration(void)
{
	struct {
		char *key;
		char *value;
		int seen;
	} entries[] = {
		{ "foo", "oof" },
		{ "bar", "rab" },
		{ "gobble", "elbbog" },
	};
	const char *key;
	char *value;
	uint32_t i, n;
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		cl_git_pass(git_hashmap_test_put(&g_table, entries[i].key, entries[i].value));

	i = 0, n = 0;
	while (git_hashmap_test_iterate(&iter, &key, &value, &g_table) == 0) {
		size_t j;

		for (j = 0; j < ARRAY_SIZE(entries); j++) {
			if (strcmp(entries[j].key, key))
				continue;

			cl_assert_equal_i(entries[j].seen, 0);
			cl_assert_equal_s(entries[j].value, value);
			entries[j].seen++;
			break;
		}

		n++;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		cl_assert_equal_i(entries[i].seen, 1);

	cl_assert_equal_i(n, ARRAY_SIZE(entries));
}

void test_hashmap__iterating_empty_map_stops_immediately(void)
{
	git_hashmap_iter_t iter = GIT_HASHMAP_ITER_INIT;

	cl_git_fail_with(GIT_ITEROVER, git_hashmap_test_iterate(&iter, NULL, NULL, &g_table));
}
