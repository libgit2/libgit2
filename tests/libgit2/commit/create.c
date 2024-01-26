#include "clar_libgit2.h"
#include "repository.h"

/* Fixture setup */
static git_repository *g_repo;
static git_signature *g_author, *g_committer;

void test_commit_create__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo2");
	cl_git_pass(git_signature_new(&g_author, "Edward Thomson", "ethomson@edwardthomson.com", 123456789, 60));
	cl_git_pass(git_signature_new(&g_committer, "libgit2 user", "nobody@noreply.libgit2.org", 987654321, 90));
}

void test_commit_create__cleanup(void)
{
	git_signature_free(g_committer);
	git_signature_free(g_author);
	cl_git_sandbox_cleanup();
}


void test_commit_create__from_stage_simple(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_index *index;
	git_oid commit_id;
	git_tree *tree;

	opts.author = g_author;
	opts.committer = g_committer;

	cl_git_rewritefile("testrepo2/newfile.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/newfile2.txt", "This is a new file.\n");
	cl_git_rewritefile("testrepo2/README", "hello, world.\n");
	cl_git_rewritefile("testrepo2/new.txt", "hi there.\n");

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "newfile2.txt"));
	cl_git_pass(git_index_add_bypath(index, "README"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_commit_create_from_stage(&commit_id, g_repo, "This is the message.", &opts));

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	cl_assert_equal_oidstr("241b5b04e847bc38dd7b4b9f49f21e55da40f3a6", &commit_id);
	cl_assert_equal_oidstr("b27210772d0633870b4f486d04ed3eb5ebbef5e7", git_tree_id(tree));

	git_index_free(index);
	git_tree_free(tree);
}

void test_commit_create__from_stage_nochanges(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_oid commit_id;
	git_tree *tree;

	opts.author = g_author;
	opts.committer = g_committer;

	cl_git_fail_with(GIT_EUNCHANGED, git_commit_create_from_stage(&commit_id, g_repo, "Message goes here.", &opts));

	opts.allow_empty_commit = 1;

	cl_git_pass(git_commit_create_from_stage(&commit_id, g_repo, "Message goes here.", &opts));

	cl_git_pass(git_repository_head_tree(&tree, g_repo));

	cl_assert_equal_oidstr("f776dc4c7fd8164b7127dc8e4f9b44421cb01b56", &commit_id);
	cl_assert_equal_oidstr("c4dc1555e4d4fa0e0c9c3fc46734c7c35b3ce90b", git_tree_id(tree));

	git_tree_free(tree);
}

void test_commit_create__from_stage_newrepo(void)
{
	git_commit_create_options opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_repository *newrepo;
	git_index *index;
	git_commit *commit;
	git_tree *tree;
	git_oid commit_id;

	opts.author = g_author;
	opts.committer = g_committer;

	git_repository_init(&newrepo, "newrepo", false);
	cl_git_pass(git_repository_index(&index, newrepo));

	cl_git_rewritefile("newrepo/hello.txt", "hello, world.\n");
	cl_git_rewritefile("newrepo/hi.txt", "hi there.\n");
	cl_git_rewritefile("newrepo/foo.txt", "bar.\n");

	cl_git_pass(git_index_add_bypath(index, "hello.txt"));
	cl_git_pass(git_index_add_bypath(index, "foo.txt"));
	cl_git_pass(git_index_write(index));

	cl_git_pass(git_commit_create_from_stage(&commit_id, newrepo, "Initial commit.", &opts));
	cl_git_pass(git_repository_head_commit(&commit, newrepo));
	cl_git_pass(git_repository_head_tree(&tree, newrepo));

	cl_assert_equal_oid(&commit_id, git_commit_id(commit));
	cl_assert_equal_oidstr("b2fa96a4f191c76eb172437281c66aa29609dcaa", git_commit_tree_id(commit));

	git_tree_free(tree);
	git_commit_free(commit);
	git_index_free(index);
	git_repository_free(newrepo);
	cl_fixture_cleanup("newrepo");
}
