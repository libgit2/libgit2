#include "clar_libgit2.h"

#include "futils.h"
#include "grafts.h"
#include "repository.h"

#include "git2/worktree.h"

static git_repository *g_repo;
static git_oid g_shallow_oid;

void test_grafts_shallow__initialize(void)
{
	cl_git_pass(git_oid_from_string(&g_shallow_oid, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644", GIT_OID_SHA1));
}

void test_grafts_shallow__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("wt");
}

void test_grafts_shallow__no_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_grafts_shallow__empty_shallow_file(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_mkfile("testrepo.git/shallow", "");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
}

void test_grafts_shallow__shallow_repo(void)
{
	g_repo = cl_git_sandbox_init("shallow.git");
	cl_assert_equal_i(1, git_repository_is_shallow(g_repo));
}

void test_grafts_shallow__clears_errors(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_assert_equal_i(0, git_repository_is_shallow(g_repo));
	cl_assert_equal_i(GIT_ERROR_NONE, git_error_last()->klass);
}

void test_grafts_shallow__shallow_oids(void)
{
	git_commit_graft *graft;
	git_grafts *grafts;

	g_repo = cl_git_sandbox_init("shallow.git");

	cl_git_pass(git_repository_shallow_grafts__weakptr(&grafts, g_repo));
	cl_assert_equal_i(1, git_grafts_size(grafts));
	cl_git_pass(git_grafts_get(&graft, grafts, &g_shallow_oid));
}

void test_grafts_shallow__cache_clearing(void)
{
	git_commit_graft *graft;
	git_grafts *grafts;
	git_oid tmp_oid;

	cl_git_pass(git_oid_from_string(&tmp_oid, "0000000000000000000000000000000000000000", GIT_OID_SHA1));
	g_repo = cl_git_sandbox_init("shallow.git");
	cl_git_pass(git_repository_shallow_grafts__weakptr(&grafts, g_repo));

	cl_assert_equal_i(1, git_grafts_size(grafts));
	cl_git_pass(git_grafts_get(&graft, grafts, &g_shallow_oid));

	cl_git_mkfile("shallow.git/shallow",
		"be3563ae3f795b2b4353bcce3a527ad0a4f7f644\n"
		"0000000000000000000000000000000000000000\n"
	);

	cl_git_pass(git_grafts_refresh(grafts));
	cl_assert_equal_i(2, git_grafts_size(grafts));
	cl_git_pass(git_grafts_get(&graft, grafts, &g_shallow_oid));
	cl_git_pass(git_grafts_get(&graft, grafts, &tmp_oid));

	cl_git_pass(p_unlink("shallow.git/shallow"));
	cl_git_pass(git_grafts_refresh(grafts));
	cl_assert_equal_i(0, git_grafts_size(grafts));
}

void test_grafts_shallow__errors_on_borked(void)
{
	git_grafts *grafts;

	g_repo = cl_git_sandbox_init("shallow.git");

	cl_git_mkfile("shallow.git/shallow", "lolno");
	cl_git_pass(git_repository_shallow_grafts__weakptr(&grafts, g_repo));
	cl_git_fail(git_grafts_refresh(grafts));
	cl_assert_equal_i(0, git_grafts_size(grafts));

	cl_git_mkfile("shallow.git/shallow", "lolno\n");
	cl_git_pass(git_repository_shallow_grafts__weakptr(&grafts, g_repo));
	cl_git_fail(git_grafts_refresh(grafts));
	cl_assert_equal_i(0, git_grafts_size(grafts));
}

void test_grafts_shallow__revwalk_behavior(void)
{
	git_revwalk *w;
	git_oid oid_1, oid_2, oid_3;

	g_repo = cl_git_sandbox_init("shallow.git");

	cl_git_pass(git_revwalk_new(&w, g_repo));
	cl_git_pass(git_revwalk_push_head(w));

	cl_git_pass(git_revwalk_next(&oid_1, w)); /* a65fedf39aefe402d3bb6e24df4d4f5fe4547750 */
	cl_git_pass(git_revwalk_next(&oid_2, w)); /* be3563ae3f795b2b4353bcce3a527ad0a4f7f644 */
	cl_git_fail_with(GIT_ITEROVER, git_revwalk_next(&oid_3, w));

	cl_assert_equal_s(git_oid_tostr_s(&oid_1), "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	cl_assert_equal_s(git_oid_tostr_s(&oid_2), "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");

	git_revwalk_free(w);
}

void test_grafts_shallow__grafted_object(void)
{
	git_commit *commit;

	g_repo = cl_git_sandbox_init("shallow.git");

	cl_git_pass(git_commit_lookup(&commit, g_repo, &g_shallow_oid));

	cl_assert_equal_i(0, git_commit_parentcount(commit));

	git_commit_free(commit);
}

static void add_worktree(git_repository **wt_repo, git_worktree **wt)
{
	git_str path = GIT_STR_INIT;

	cl_git_pass(git_str_joinpath(&path, git_repository_path(g_repo), "../wt"));
	cl_git_pass(git_worktree_add(wt, g_repo, "wt", path.ptr, NULL));
	cl_git_pass(git_repository_open_from_worktree(wt_repo, *wt));

	git_str_dispose(&path);
}

void test_grafts_shallow__worktree_reads_shallow_from_commondir(void)
{
	git_repository *wt_repo;
	git_worktree *wt;
	git_revwalk *walk;
	git_oid oid;
	size_t count = 0;
	int error;

	g_repo = cl_git_sandbox_init("shallow.git");
	add_worktree(&wt_repo, &wt);

	cl_git_pass(git_revwalk_new(&walk, wt_repo));
	cl_git_pass(git_revwalk_push_head(walk));
	while ((error = git_revwalk_next(&oid, walk)) == 0)
		count++;
	cl_assert_equal_i(GIT_ITEROVER, error);
	cl_assert_equal_i(2, count);

	git_revwalk_free(walk);
	git_repository_free(wt_repo);
	git_worktree_free(wt);
}

void test_grafts_shallow__worktree_writes_shallow_to_commondir(void)
{
	git_repository *wt_repo;
	git_worktree *wt;
	git_oidarray roots = { &g_shallow_oid, 1 };
	git_str contents = GIT_STR_INIT;

	g_repo = cl_git_sandbox_init("testrepo.git");
	add_worktree(&wt_repo, &wt);

	cl_git_pass(git_repository__shallow_roots_write(wt_repo, &roots));

	cl_git_pass(git_futils_readbuffer(&contents, "testrepo.git/shallow"));
	cl_assert_equal_s("be3563ae3f795b2b4353bcce3a527ad0a4f7f644\n", contents.ptr);
	cl_assert(!git_fs_path_exists("testrepo.git/worktrees/wt/shallow"));

	git_str_dispose(&contents);
	git_repository_free(wt_repo);
	git_worktree_free(wt);
}
