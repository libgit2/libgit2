#include "clar_libgit2.h"
#include "alloc.h"
#include "config_list.h"

static git_allocator original_allocator;
static size_t allocation_count, fail_at;
static bool allocation_failed;

static bool fail_allocation(void)
{
	if (allocation_count++ != fail_at)
		return false;

	allocation_failed = true;
	return true;
}

static void *failing_malloc(size_t size, const char *file, int line)
{
	return fail_allocation() ? NULL : original_allocator.gmalloc(size, file, line);
}

static void *failing_realloc(void *ptr, size_t size, const char *file, int line)
{
	return fail_allocation() ? NULL : original_allocator.grealloc(ptr, size, file, line);
}

void test_config_list__initialize(void)
{
	original_allocator = git__allocator;
}

void test_config_list__cleanup(void)
{
	cl_git_pass(git_allocator_setup(&original_allocator));
}

static git_config_list_entry *new_entry(
	git_config_list *list, const char *name, const char *value)
{
	git_config_list_entry *entry = git__calloc(1, sizeof(*entry));

	cl_assert(entry);
	cl_assert(entry->base.entry.name = git__strdup(name));
	cl_assert(entry->base.entry.value = git__strdup(value));
	entry->base.free = git_config_list_entry_free;
	entry->config_list = list;
	return entry;
}

static void assert_entry_count(git_config_list *list, size_t expected)
{
	git_config_iterator *iter;
	git_config_entry *entry;
	size_t count = 0;
	int error;

	cl_git_pass(git_config_list_iterator_new(&iter, list));
	while ((error = git_config_next(&entry, iter)) == 0)
		count++;
	cl_assert_equal_i(GIT_ITEROVER, error);
	cl_assert_equal_sz(expected, count);
	git_config_iterator_free(iter);
}

static void assert_append_failures(size_t existing, bool multivar)
{
	git_allocator allocator = original_allocator;
	git_config_list *list;
	git_config_list_entry *entry, *found;
	const char *name = multivar ? "test.key0" : "test.new";
	const char *original_name, *original_value;
	char key[32];
	size_t i;
	int error, error_class;

	allocator.gmalloc = failing_malloc;
	allocator.grealloc = failing_realloc;

	/* Fail only allocations made by append, including hash map growth. */
	for (fail_at = 0; fail_at < 16; fail_at++) {
		cl_git_pass(git_config_list_new(&list));
		for (i = 0; i < existing; i++) {
			p_snprintf(key, sizeof(key), "test.key%u", (unsigned int)i);
			entry = new_entry(list, key, "original");
			cl_git_pass(git_config_list_append(list, entry));
		}

		entry = new_entry(list, name, "appended");
		original_name = entry->base.entry.name;
		original_value = entry->base.entry.value;
		allocation_count = 0;
		allocation_failed = false;

		cl_git_pass(git_allocator_setup(&allocator));
		error = git_config_list_append(list, entry);
		error_class = error < 0 ? git_error_last()->klass : 0;
		cl_git_pass(git_allocator_setup(&original_allocator));

		if (allocation_failed) {
			cl_git_fail(error);
			cl_assert_equal_i(GIT_ERROR_NOMEMORY, error_class);

			/* Failure must not consume or modify the caller's entry. */
			cl_assert(entry->base.entry.name == original_name);
			cl_assert(entry->base.entry.value == original_value);
			cl_assert_equal_s(name, entry->base.entry.name);
			cl_assert_equal_s("appended", entry->base.entry.value);
			assert_entry_count(list, existing);

			/* Existing keys must remain unique and retain their values. */
			for (i = 0; i < existing; i++) {
				p_snprintf(key, sizeof(key), "test.key%u", (unsigned int)i);
				cl_git_pass(git_config_list_get_unique(&found, list, key));
				cl_assert_equal_s("original", found->base.entry.value);
			}
			if (!multivar)
				cl_git_fail_with(GIT_ENOTFOUND,
					git_config_list_get(&found, list, name));

			/* The same entry can be retried after any failed allocation. */
			cl_git_pass(git_config_list_append(list, entry));
		} else {
			cl_git_pass(error);
		}

		cl_git_pass(git_config_list_get(&found, list, name));
		cl_assert(found == entry);
		if (multivar)
			cl_git_fail(git_config_list_get_unique(&found, list, name));
		else
			cl_git_pass(git_config_list_get_unique(&found, list, name));
		assert_entry_count(list, existing + 1);
		git_config_list_free(list);

		if (!allocation_failed)
			break;
	}

	cl_assert(fail_at > 0 && fail_at < 16);
}

void test_config_list__append_new_key_allocation_failures(void)
{
	assert_append_failures(0, false);
}

void test_config_list__append_growing_map_allocation_failures(void)
{
	/* Three distinct entries fill the initial hash map to its load limit. */
	assert_append_failures(3, false);
}

void test_config_list__append_multivar_allocation_failure(void)
{
	assert_append_failures(1, true);
}
