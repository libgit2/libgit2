#include "clar_libgit2.h"
#include "repository.h"

/* Fixture setup */
static git_repository *g_repo;
static git_signature *g_committer;

void test_commit_amend__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo2");
	cl_git_pass(git_signature_new(&g_committer, "libgit2 user", "nobody@noreply.libgit2.org", 987654321, 90));
}

void test_commit_amend__cleanup(void)
{
	git_signature_free(g_committer);
	cl_git_sandbox_cleanup();
}


void test_commit_amend__from_stage_simple(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_index *index;
	git_oid commit_id;
	git_tree *tree;

	opts.committer = g_committer;

	cl_git_rewritefile("testrepo2/newfile.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/newfile2.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/README", "hello, world.\n");
	cl_git_rewritefile("testrepo2/new.txt", "hi there.\n");

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "newfile2.txt"));
	cl_git_pass(git_index_add_bypath(index, "README"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_commit_amend_from_stage(&commit_id, g_repo, NULL, &opts));

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	cl_assert_equal_oidstr("63ec0b083fd14c22a68fd2b1794f26e4b396b6b3", &commit_id);
	cl_assert_equal_oidstr("b27210772d0633870b4f486d04ed3eb5ebbef5e7", git_tree_id(tree));

	git_index_free(index);
	git_tree_free(tree);
}

void test_commit_amend__from_stage_newmessage(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_oid commit_id;
	git_tree *tree;

	opts.committer = g_committer;

	cl_git_pass(git_commit_amend_from_stage(&commit_id, g_repo, "New message goes here.", &opts));

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	cl_assert_equal_oidstr("8b0e1cacc8380023705192466aaef8a15ddae7b3", &commit_id);
	cl_assert_equal_oidstr("c4dc1555e4d4fa0e0c9c3fc46734c7c35b3ce90b", git_tree_id(tree));

	git_tree_free(tree);
}

void test_commit_amend__from_stage_nochanges(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_oid commit_id;
	git_tree *tree;

	opts.committer = g_committer;

	cl_git_pass(git_commit_amend_from_stage(&commit_id, g_repo, NULL, &opts));

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	cl_assert_equal_oidstr("da86907c6d505a92c5683bece08f23d68ac785bd", &commit_id);
	cl_assert_equal_oidstr("c4dc1555e4d4fa0e0c9c3fc46734c7c35b3ce90b", git_tree_id(tree));

	git_tree_free(tree);
}

void test_commit_amend__from_tree(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_index *index;
	git_oid commit_id;
	git_oid tree_id;
	git_tree *tree, *lookedup;

	opts.committer = g_committer;

	cl_git_rewritefile("testrepo2/newfile.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/newfile2.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/README", "hello, world.\n");
	cl_git_rewritefile("testrepo2/new.txt", "hi there.\n");

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "newfile2.txt"));
	cl_git_pass(git_index_add_bypath(index, "README"));
	cl_git_pass(git_index_write(index));
	cl_git_pass(git_index_write_tree(&tree_id, index));

	cl_assert_equal_oidstr("b27210772d0633870b4f486d04ed3eb5ebbef5e7", &tree_id);

	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));

	cl_git_pass(git_commit_amend_from_tree(&commit_id, g_repo, tree, NULL, &opts));

	cl_git_pass(git_repository_head_tree(&lookedup, g_repo));

	cl_assert_equal_oidstr("63ec0b083fd14c22a68fd2b1794f26e4b396b6b3", &commit_id);
	cl_assert_equal_oidstr("b27210772d0633870b4f486d04ed3eb5ebbef5e7", git_tree_id(lookedup));

	git_index_free(index);
	git_tree_free(tree);
	git_tree_free(lookedup);
}
