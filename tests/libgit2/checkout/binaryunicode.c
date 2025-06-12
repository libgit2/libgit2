#include "clar_libgit2.h"
#include "refs.h"
#include "repo/repo_helpers.h"
#include "path.h"
#include "futils.h"
#include "odb.h"

static git_repository *g_repo;

void test_checkout_binaryunicode__initialize(void)
{
	g_repo = cl_git_sandbox_init("binaryunicode");
}

void test_checkout_binaryunicode__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void execute_test(void)
{
	git_oid oid, check;
	git_commit *commit;
	git_tree *tree;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

	cl_git_pass(git_reference_name_to_id(&oid, g_repo, "refs/heads/branch1"));
	cl_git_pass(git_commit_lookup(&commit, g_repo, &oid));
	cl_git_pass(git_commit_tree(&tree, commit));

	cl_git_pass(git_checkout_tree(g_repo, (git_object *)tree, &opts));

	git_tree_free(tree);
	git_commit_free(commit);

	/* Verify that the lenna.jpg file was checked out correctly */
	cl_git_pass(git_oid_from_string(&check, "8ab005d890fe53f65eda14b23672f60d9f4ec5a1", GIT_OID_SHA1));
	cl_git_pass(git_object_id_from_file(&oid, "binaryunicode/lenna.jpg", NULL));
	cl_assert_equal_oid(&oid, &check);

	/* Verify that the text file was checked out correctly */
	cl_git_pass(git_oid_from_string(&check, "965b223880dd4249e2c66a0cc0b4cffe1dc40f5a", GIT_OID_SHA1));
	cl_git_pass(git_object_id_from_file(&oid, "binaryunicode/utf16_withbom_noeol_crlf.txt", NULL));
	cl_assert_equal_oid(&oid, &check);
}

void test_checkout_binaryunicode__noautocrlf(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	execute_test();
}

void test_checkout_binaryunicode__autocrlf(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	execute_test();
}
