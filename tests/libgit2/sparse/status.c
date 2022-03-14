#include "clar_libgit2.h"
#include "futils.h"
#include "git2/attr.h"
#include "sparse.h"
#include "status/status_helpers.h"

static git_repository *g_repo = NULL;

void test_sparse_status__initialize(void)
{
}

void test_sparse_status__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void assert_ckeckout_(
	bool expected, const char *filepath,
	const char *file, const char *func, int line)
{
	int checkout = 0;
	cl_git_expect(
	 git_sparse_check_path(&checkout, g_repo, filepath), 0, file, func, line);
	clar__assert(
	 (expected != 0) == (checkout != 0),
	 file, func, line, "expected != checkout", filepath, 1);
}

#define assert_checkout(expected, filepath) \
assert_ckeckout_(expected, filepath, __FILE__, __func__, __LINE__)
#define assert_is_checkout(filepath) \
assert_ckeckout_(true, filepath, __FILE__, __func__, __LINE__)
#define refute_is_checkout(filepath) \
assert_ckeckout_(false, filepath, __FILE__, __func__, __LINE__)

#define define_test_cases \
struct test_case{ \
	const char *path; \
	int expected; \
} test_cases[] = { \
	/* include all pattern from info/sparse-checkout */ \
	{ "file1", 1 }, \
	{ "file2", 1 }, \
	{ "file11.txt", 1 }, \
	\
	/* exclude subfolder pattern from info/sparse-checkout */ \
	{ "a/", 0 }, \
	{ "a/file3", 0 }, \
	{ "a/file4", 0 }, \
	\
	{ "b/", 0 }, \
	{ "b/file12.txt", 0 }, \
	{ "b/file5", 0 }, \
	{ "b/file6", 0 }, \
	\
	{ "b/c/", 0 }, \
	{ "b/c/file7", 0 }, \
	{ "b/c/file8", 0 }, \
 	\
	{ "b/d/", 0 }, \
	{ "b/d/file10", 0 }, \
	{ "b/d/file9", 0 }, \
	\
	{ NULL, 0 } \
}, *one_test; \

void test_sparse_status__0(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));

	git_attr_cache_flush(g_repo);

	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);

	/* confirm that sparse-checkout file is cached */
	cl_assert(git_attr_cache__is_cached(
		g_repo, GIT_ATTR_FILE_SOURCE_FILE, ".git/info/sparse-checkout"));
}

static const char* paths[] = {
	"file1",
	"file2",
	"file11.txt",
	"a/",
	"a/file3",
	"a/file4",
	"b/",
	"b/file12.txt",
	"b/file5",
	"b/file6",
	"b/c/",
	"b/c/file7",
	"b/c/file8",
	"b/d/",
	"b/d/file10",
	"b/d/file9",
	NULL
};

void test_sparse_status__disabled(void)
{
	const char** path;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_pass(git_sparse_checkout_disable(g_repo));
	
	for (path = paths; *path != NULL; path++)
		assert_is_checkout(*path);
}

void test_sparse_status__full_checkout(void)
{
	const char** path;
	g_repo = cl_git_sandbox_init("sparse");
	{
		char *pattern_strings[] = { "/*" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));
	}

	for (path = paths; *path != NULL; path++)
		assert_is_checkout(*path);
}

void test_sparse_status__no_checkout(void)
{
	const char** path;
	g_repo = cl_git_sandbox_init("sparse");
	{
		char *pattern_strings[] = { "!/*" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_set(g_repo, &patterns));
	}
	
	for (path = paths; *path != NULL; path++)
		refute_is_checkout(*path);
}

void test_sparse_status__no_sparse_file(void)
{
	const char** path;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	cl_git_rmfile("sparse/.git/info/sparse-checkout");
	
	for (path = paths; *path != NULL; path++)
		refute_is_checkout(*path);
}

void test_sparse_status__append_folder(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "/a/" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}

	test_cases[3].expected = 1;
	test_cases[4].expected = 1;
	test_cases[5].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__ignore_subfolders(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "/b/", "!/b/*/" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	test_cases[6].expected = 1;
	test_cases[7].expected = 1;
	test_cases[8].expected = 1;
	test_cases[9].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__append_file(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "/b/c/file7" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	test_cases[11].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__append_suffix(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "*.txt" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	test_cases[7].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__exclude_single_file_suffix(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "*.txt", "!file11.txt" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	test_cases[2].expected = 0;
	test_cases[7].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__match_wildcard(void)
{
	define_test_cases
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "file1*" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	test_cases[7].expected = 1;
	test_cases[14].expected = 1;
	
	for (one_test = test_cases; one_test->path != NULL; one_test++)
		assert_checkout(one_test->expected, one_test->path);
}

void test_sparse_status__clean(void)
{
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(0, st.count);
}

void test_sparse_status__clean_unmodified(void)
{
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	memset(&st, 0, sizeof(st));
	
	opts.flags = GIT_STATUS_OPT_DEFAULTS | GIT_STATUS_OPT_INCLUDE_UNMODIFIED;
	cl_git_pass(git_status_foreach_ext(g_repo, &opts, cb_status__single, &st));
	cl_assert_equal_i(12, st.count);
	cl_assert(st.status == GIT_STATUS_CURRENT);
}

void test_sparse_status__new_file(void)
{
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	cl_git_mkfile("sparse/newfile", "/hello world\n");
	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_WT_NEW);
	
	assert_is_checkout("newfile");
}

void test_sparse_status__new_file_new_folder(void)
{
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	cl_must_pass(git_futils_mkdir("sparse/new", 0777, 0));
	cl_git_mkfile("sparse/new/newfile", "/hello world\n");
	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_WT_NEW);
	
	refute_is_checkout("new/newfile");
}

void test_sparse_status__new_file_sparse_folder(void)
{
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/newfile", "/hello world\n");
	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(1, st.count);
	cl_assert(st.status == GIT_STATUS_WT_NEW);
	
	refute_is_checkout("new/newfile");
}

void test_sparse_status__new_sparse_file_sparse_folder(void)
{
	status_entry_single st;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	
	cl_must_pass(git_futils_mkdir("sparse/a", 0777, 0));
	cl_git_mkfile("sparse/a/file3", "/hello world\n");
	memset(&st, 0, sizeof(st));
	cl_git_pass(git_status_foreach(g_repo, cb_status__single, &st));
	cl_assert_equal_i(0, st.count);
	
	refute_is_checkout("new/newfile");
}

void test_sparse_status__ignorecase(void)
{
	bool ignore_case;
	git_index *index;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(g_repo, &scopts));
	{
		char *pattern_strings[] = { "/b/file5" };
		git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };
		cl_git_pass(git_sparse_checkout_add(g_repo, &patterns));
	}
	
	cl_must_pass(git_futils_mkdir("sparse/b", 0777, 0));
	cl_git_mkfile("sparse/b/File5", "/hello world\n");
	
	cl_git_pass(git_repository_index(&index, g_repo));
	ignore_case = (git_index_caps(index) & GIT_INDEX_CAPABILITY_IGNORE_CASE) != 0;
	git_index_free(index);
	
	if (ignore_case)
		assert_is_checkout("b/File5");
	else
		refute_is_checkout("b/File5");
	
	git_index_free(index);
}
