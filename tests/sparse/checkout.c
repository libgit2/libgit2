#include "clar_libgit2.h"
#include "futils.h"
#include "sparse.h"
#include "git2/checkout.h"

static git_repository *g_repo = NULL;

void test_sparse_checkout__initialize(void)
{
}

void test_sparse_checkout__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

typedef struct
{
	int count;
	unsigned int why;
	git_checkout_perfdata perfdata;
} checkout_payload;

static int checkout_callback(
	git_checkout_notify_t why,
	const char *path,
	const git_diff_file *baseline,
	const git_diff_file *target,
	const git_diff_file *workdir,
	void *payload)
{
	checkout_payload* cp = (checkout_payload*) payload;
	cl_assert(cp);
	GIT_UNUSED(path);
	GIT_UNUSED(baseline);
	GIT_UNUSED(target);
	GIT_UNUSED(workdir);
	
	cp->count++;
	cp->why |= why;
	return 0;
}

static void checkout_perf_callback(const git_checkout_perfdata *perfdata, void *payload)
{
	checkout_payload* cp = (checkout_payload*) payload;
	cl_assert(cp);
	cp->perfdata.chmod_calls += perfdata->chmod_calls;
	cp->perfdata.mkdir_calls += perfdata->mkdir_calls;
	cp->perfdata.stat_calls += perfdata->stat_calls;
}

static void setup_options(git_checkout_options* opts, void* payload)
{
	opts->checkout_strategy = GIT_CHECKOUT_FORCE;
	
	if (payload != NULL) {
		opts->notify_cb = &checkout_callback;
		opts->notify_payload = payload;
		opts->notify_flags = GIT_CHECKOUT_NOTIFY_ALL;
		
		opts->perfdata_cb = &checkout_perf_callback;
		opts->perfdata_payload = payload;
	}
}

void test_sparse_checkout__skips_sparse_files(void)
{
	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(&scopts, g_repo));

	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, (void*) &payload);
	
	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	cl_assert_equal_i(payload.count, 0);
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 0);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);
	cl_assert_equal_i(payload.perfdata.stat_calls, 0);

	cl_assert(git_path_exists("sparse/file1"));

	git_object_free(object);
}

void test_sparse_checkout__checksout_files(void)
{
	char* pattern_strings[] = { "/a/" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(&scopts, g_repo));
	cl_git_pass(git_sparse_checkout_add(&patterns, g_repo));
	
	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, (void*) &payload);
	
	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	// 2x dirty, 2x update
	cl_assert_equal_i(payload.count, 2 + 2);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY | GIT_CHECKOUT_NOTIFY_UPDATED);
	
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 1);
	cl_assert_equal_i(payload.perfdata.stat_calls, 5);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);

	cl_assert(git_path_exists("sparse/a/file3"));

	git_object_free(object);
}

void test_sparse_checkout__checksout_all_files(void)
{
    char *pattern_strings[] = { "/*" };
    git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	g_repo = cl_git_sandbox_init("sparse");

    cl_git_pass(git_sparse_checkout_set(&patterns, g_repo));

	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, (void*) &payload);
	
	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	// 9x dirty, 9x update
	cl_assert_equal_i(payload.count, 9 + 9);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY | GIT_CHECKOUT_NOTIFY_UPDATED);
	
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 4);
	cl_assert_equal_i(payload.perfdata.stat_calls, 22);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);

	cl_assert(git_path_exists("sparse/file1"));
	cl_assert(git_path_exists("sparse/a/file3"));
	cl_assert(git_path_exists("sparse/b/file5"));
	cl_assert(git_path_exists("sparse/b/c/file7"));
	cl_assert(git_path_exists("sparse/b/d/file9"));

	git_object_free(object);
}

void test_sparse_checkout__updates_index(void)
{
	char *pattern_strings[] = { "/*" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_index_iterator* iterator;
	git_index* index;
	const git_index_entry *entry;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_set(&patterns, g_repo));
	
	setup_options(&opts, NULL);
	
	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_iterator_new(&iterator, index));
	while (git_index_iterator_next(&entry, iterator) != GIT_ITEROVER)
		cl_assert_equal_i(entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE, 0);
	
	git_object_free(object);
	git_index_iterator_free(iterator);
	git_index_free(index);
}

void test_sparse_checkout__keeps_sparse_files(void)
{
	char *pattern_strings[] = { "/*" };
	git_strarray patterns = { pattern_strings, ARRAY_SIZE(pattern_strings) };

	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(&scopts, g_repo));
	cl_git_pass(git_sparse_checkout_disable(g_repo));

	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	
	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, &payload);
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));

	cl_assert_equal_i(payload.count, 9 + 9);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY | GIT_CHECKOUT_NOTIFY_UPDATED);

	cl_git_pass(git_sparse_checkout_set(&patterns, g_repo));

	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, &payload);
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	cl_assert_equal_i(payload.count, 0);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_NONE);
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 0);
	cl_assert_equal_i(payload.perfdata.stat_calls, 0);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);

	cl_assert(git_path_exists("sparse/file1"));
	
	git_object_free(object);
}

void test_sparse_checkout__removes_sparse_files(void)
{
	git_object* object;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_revparse_single(&object, g_repo, "HEAD"));
	
	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, &payload);
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	cl_assert_equal_i(payload.count, 9 + 9);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY | GIT_CHECKOUT_NOTIFY_UPDATED);

	cl_git_pass(git_sparse_checkout_init(&scopts, g_repo));
	
	memset(&payload, 0, sizeof(payload));
	setup_options(&opts, &payload);
	opts.checkout_strategy |= GIT_CHECKOUT_REMOVE_SPARSE_FILES;
	cl_git_pass(git_checkout_tree(g_repo, object, &opts));
	
	cl_assert_equal_i(payload.count, 9);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY);
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 0);
	cl_assert_equal_i(payload.perfdata.stat_calls, 0);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);

	cl_assert_equal_b(git_path_exists("sparse/file1"), true);
	cl_assert_equal_b(git_path_exists("sparse/a/file3"), false);
	cl_assert_equal_b(git_path_exists("sparse/b/file5"), false);
	
	git_object_free(object);
}

/* matches core git behavior: git checkout-index copies all files from the index
 to the working directory regardless of the sparse-checkout ruleset. */
void test_sparse_checkout__checkout_index_sparse(void)
{
	git_index* index;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_payload payload;
	git_sparse_checkout_init_options scopts = GIT_SPARSE_CHECKOUT_INIT_OPTIONS_INIT;
	g_repo = cl_git_sandbox_init("sparse");

	cl_git_pass(git_sparse_checkout_init(&scopts, g_repo));

    memset(&payload, 0, sizeof(payload));
	setup_options(&opts, (void*) &payload);
	
	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_checkout_index(g_repo, index, &opts));
	
	cl_assert_equal_i(payload.count, 9 + 9);
	cl_assert_equal_i(payload.why, GIT_CHECKOUT_NOTIFY_DIRTY | GIT_CHECKOUT_NOTIFY_UPDATED);
	
	cl_assert_equal_i(payload.perfdata.mkdir_calls, 4);
	cl_assert_equal_i(payload.perfdata.stat_calls, 22);
	cl_assert_equal_i(payload.perfdata.chmod_calls, 0);
	
	git_index_free(index);
}

