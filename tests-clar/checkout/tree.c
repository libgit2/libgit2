#include "clar_libgit2.h"

#include "git2/checkout.h"
#include "repository.h"
#include "buffer.h"
#include "fileops.h"

static git_repository *g_repo;
static git_checkout_opts g_opts;
static git_object *g_object;

void test_checkout_tree__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");

	GIT_INIT_STRUCTURE(&g_opts, GIT_CHECKOUT_OPTS_VERSION);
	g_opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
}

void test_checkout_tree__cleanup(void)
{
	git_object_free(g_object);
	g_object = NULL;

	cl_git_sandbox_cleanup();
}

void test_checkout_tree__cannot_checkout_a_non_treeish(void)
{
	/* blob */
	cl_git_pass(git_revparse_single(&g_object, g_repo, "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd"));

	cl_git_fail(git_checkout_tree(g_repo, g_object, NULL));
}

void test_checkout_tree__can_checkout_a_subdirectory_from_a_commit(void)
{
	char *entries[] = { "ab/de/" };

	g_opts.paths.strings = entries;
	g_opts.paths.count = 1;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees"));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/fgh/1.txt"));
}

void test_checkout_tree__can_checkout_and_remove_directory(void)
{
	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));

	/* Checkout brach "subtrees" and update HEAD, so that HEAD matches the
	 * current working tree
	 */
	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees"));
	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/subtrees"));

	cl_assert_equal_i(true, git_path_isdir("./testrepo/ab/"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/ab/de/fgh/1.txt"));

	/* Checkout brach "master" and update HEAD, so that HEAD matches the
	 * current working tree
	 */
	cl_git_pass(git_revparse_single(&g_object, g_repo, "master"));
	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/master"));

	/* This directory should no longer exist */
	cl_assert_equal_i(false, git_path_isdir("./testrepo/ab/"));
}

void test_checkout_tree__can_checkout_a_subdirectory_from_a_subtree(void)
{
	char *entries[] = { "de/" };

	g_opts.paths.strings = entries;
	g_opts.paths.count = 1;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "subtrees:ab"));

	cl_assert_equal_i(false, git_path_isdir("./testrepo/de/"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(true, git_path_isfile("./testrepo/de/2.txt"));
	cl_assert_equal_i(true, git_path_isfile("./testrepo/de/fgh/1.txt"));
}

static void progress(const char *path, size_t cur, size_t tot, void *payload)
{
	bool *was_called = (bool*)payload;
	GIT_UNUSED(path); GIT_UNUSED(cur); GIT_UNUSED(tot);
	*was_called = true;
}

void test_checkout_tree__calls_progress_callback(void)
{
	bool was_called = 0;

	g_opts.progress_cb = progress;
	g_opts.progress_payload = &was_called;

	cl_git_pass(git_revparse_single(&g_object, g_repo, "master"));

	cl_git_pass(git_checkout_tree(g_repo, g_object, &g_opts));

	cl_assert_equal_i(was_called, true);
}

void test_checkout_tree__doesnt_write_unrequested_files_to_worktree(void)
{
  git_oid master_oid;
  git_oid chomped_oid;
  git_commit* p_master_commit;
  git_commit* p_chomped_commit;
  git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

  git_oid_fromstr(&master_oid, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
  git_oid_fromstr(&chomped_oid, "e90810b8df3e80c413d903f631643c716887138d");
  cl_git_pass(git_commit_lookup(&p_master_commit, g_repo, &master_oid));
  cl_git_pass(git_commit_lookup(&p_chomped_commit, g_repo, &chomped_oid));

  /* GIT_CHECKOUT_NONE should not add any file to the working tree from the
   * index as it is supposed to be a dry run.
   */
  opts.checkout_strategy = GIT_CHECKOUT_NONE;
  git_checkout_tree(g_repo, (git_object*)p_chomped_commit, &opts);
  cl_assert_equal_i(false, git_path_isfile("testrepo/readme.txt"));
}

static void assert_on_branch(git_repository *repo, const char *branch)
{
	git_reference *head;
	git_buf bname = GIT_BUF_INIT;

	cl_git_pass(git_reference_lookup(&head, repo, GIT_HEAD_FILE));
	cl_assert_(git_reference_type(head) == GIT_REF_SYMBOLIC, branch);

	cl_git_pass(git_buf_joinpath(&bname, "refs/heads", branch));
	cl_assert_equal_s(bname.ptr, git_reference_symbolic_target(head));

	git_reference_free(head);
	git_buf_free(&bname);
}

void test_checkout_tree__can_switch_branches(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	git_oid oid;
	git_object *obj = NULL;

	assert_on_branch(g_repo, "master");

	/* do first checkout with FORCE because we don't know if testrepo
	 * base data is clean for a checkout or not
	 */
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/dir"));
	cl_git_pass(git_object_lookup(&obj, g_repo, &oid, GIT_OBJ_ANY));

	cl_git_pass(git_checkout_tree(g_repo, obj, &opts));
	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/dir"));

	cl_assert(git_path_isfile("testrepo/README"));
	cl_assert(git_path_isfile("testrepo/branch_file.txt"));
	cl_assert(git_path_isfile("testrepo/new.txt"));
	cl_assert(git_path_isfile("testrepo/a/b.txt"));

	cl_assert(!git_path_isdir("testrepo/ab"));

	assert_on_branch(g_repo, "dir");

	git_object_free(obj);

	/* do second checkout safe because we should be clean after first */
	opts.checkout_strategy = GIT_CHECKOUT_SAFE;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/subtrees"));
	cl_git_pass(git_object_lookup(&obj, g_repo, &oid, GIT_OBJ_ANY));

	cl_git_pass(git_checkout_tree(g_repo, obj, &opts));
	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/subtrees"));

	cl_assert(git_path_isfile("testrepo/README"));
	cl_assert(git_path_isfile("testrepo/branch_file.txt"));
	cl_assert(git_path_isfile("testrepo/new.txt"));
	cl_assert(git_path_isfile("testrepo/ab/4.txt"));
	cl_assert(git_path_isfile("testrepo/ab/c/3.txt"));
	cl_assert(git_path_isfile("testrepo/ab/de/2.txt"));
	cl_assert(git_path_isfile("testrepo/ab/de/fgh/1.txt"));

	cl_assert(!git_path_isdir("testrepo/a"));

	assert_on_branch(g_repo, "subtrees");

	git_object_free(obj);
}

void test_checkout_tree__can_remove_untracked(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_REMOVE_UNTRACKED;

	cl_git_mkfile("testrepo/untracked_file", "as you wish");
	cl_assert(git_path_isfile("testrepo/untracked_file"));

	cl_git_pass(git_checkout_head(g_repo, &opts));

	cl_assert(!git_path_isfile("testrepo/untracked_file"));
}

void test_checkout_tree__can_remove_ignored(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	int ignored = 0;

	opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_REMOVE_IGNORED;

	cl_git_mkfile("testrepo/ignored_file", "as you wish");

	cl_git_pass(git_ignore_add_rule(g_repo, "ignored_file\n"));

	cl_git_pass(git_ignore_path_is_ignored(&ignored, g_repo, "ignored_file"));
	cl_assert_equal_i(1, ignored);

	cl_assert(git_path_isfile("testrepo/ignored_file"));

	cl_git_pass(git_checkout_head(g_repo, &opts));

	cl_assert(!git_path_isfile("testrepo/ignored_file"));
}

/* this is essentially the code from git__unescape modified slightly */
static void strip_cr_from_buf(git_buf *buf)
{
	char *scan, *pos = buf->ptr;

	for (scan = pos; *scan; pos++, scan++) {
		if (*scan == '\r')
			scan++; /* skip '\r' */
		if (pos != scan)
			*pos = *scan;
	}

	*pos = '\0';
	buf->size = (pos - buf->ptr);
}

void test_checkout_tree__can_update_only(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	git_oid oid;
	git_object *obj = NULL;
	git_buf buf = GIT_BUF_INIT;

	/* first let's get things into a known state - by checkout out the HEAD */

	assert_on_branch(g_repo, "master");

	opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	cl_git_pass(git_checkout_head(g_repo, &opts));

	cl_assert(!git_path_isdir("testrepo/a"));

	cl_git_pass(git_futils_readbuffer(&buf, "testrepo/branch_file.txt"));
	strip_cr_from_buf(&buf);
	cl_assert_equal_s("hi\nbye!\n", buf.ptr);
	git_buf_free(&buf);

	/* now checkout branch but with update only */

	opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_UPDATE_ONLY;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/dir"));
	cl_git_pass(git_object_lookup(&obj, g_repo, &oid, GIT_OBJ_ANY));

	cl_git_pass(git_checkout_tree(g_repo, obj, &opts));
	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/dir"));

	assert_on_branch(g_repo, "dir");

	/* this normally would have been created (which was tested separately in
	 * the test_checkout_tree__can_switch_branches test), but with
	 * UPDATE_ONLY it will not have been created.
	 */
	cl_assert(!git_path_isdir("testrepo/a"));

	/* but this file still should have been updated */
	cl_git_pass(git_futils_readbuffer(&buf, "testrepo/branch_file.txt"));
	strip_cr_from_buf(&buf);
	cl_assert_equal_s("hi\n", buf.ptr);

	git_buf_free(&buf);

	git_object_free(obj);
}
