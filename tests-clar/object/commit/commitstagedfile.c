#include "clar_libgit2.h"
#include "posix.h"

static git_repository *repo;

void test_object_commit_commitstagedfile__initialize(void)
{
	cl_fixture("treebuilder");
	cl_git_pass(git_repository_init(&repo, "treebuilder/", 0));
	cl_assert(repo != NULL);
}

void test_object_commit_commitstagedfile__cleanup(void)
{
	git_repository_free(repo);
	repo = NULL;

	cl_fixture_cleanup("treebuilder");
}

void test_object_commit_commitstagedfile__generate_predictable_object_ids(void)
{
	git_index *index;
	const git_index_entry *entry;
	git_oid expected_blob_oid, tree_oid, expected_tree_oid, commit_oid, expected_commit_oid;
	git_signature *signature;
	git_tree *tree;
	char buffer[128];

	/*
	 * The test below replicates the following git scenario
	 *
	 * $ echo "test" > test.txt
	 * $ git hash-object test.txt
	 * 9daeafb9864cf43055ae93beb0afd6c7d144bfa4
	 *
	 * $ git add .
	 * $ git commit -m "Initial commit"
	 *
	 * $ git log
	 * commit 1fe3126578fc4eca68c193e4a3a0a14a0704624d
	 * Author: nulltoken <emeric.fermas@gmail.com>
	 * Date:   Wed Dec 14 08:29:03 2011 +0100
	 *
	 *     Initial commit
	 *
	 * $ git show 1fe3 --format=raw
	 * commit 1fe3126578fc4eca68c193e4a3a0a14a0704624d
	 * tree 2b297e643c551e76cfa1f93810c50811382f9117
	 * author nulltoken <emeric.fermas@gmail.com> 1323847743 +0100
	 * committer nulltoken <emeric.fermas@gmail.com> 1323847743 +0100
	 * 
	 *     Initial commit
	 * 
	 * diff --git a/test.txt b/test.txt
	 * new file mode 100644
	 * index 0000000..9daeafb
	 * --- /dev/null
	 * +++ b/test.txt
	 * @@ -0,0 +1 @@
	 * +test
	 *
	 * $ git ls-tree 2b297
	 * 100644 blob 9daeafb9864cf43055ae93beb0afd6c7d144bfa4    test.txt
	 */

	cl_git_pass(git_oid_fromstr(&expected_commit_oid, "1fe3126578fc4eca68c193e4a3a0a14a0704624d"));
	cl_git_pass(git_oid_fromstr(&expected_tree_oid, "2b297e643c551e76cfa1f93810c50811382f9117"));
	cl_git_pass(git_oid_fromstr(&expected_blob_oid, "9daeafb9864cf43055ae93beb0afd6c7d144bfa4"));

	/*
	 * Add a new file to the index
	 */
	cl_git_mkfile("treebuilder/test.txt", "test\n");
	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_add_bypath(index, "test.txt"));

	entry = git_index_get_byindex(index, 0);

	cl_assert(git_oid_cmp(&expected_blob_oid, &entry->oid) == 0);

	/*
	 * Information about index entry should match test file
	 */
	{
		struct stat st;
		cl_must_pass(p_lstat("treebuilder/test.txt", &st));
		cl_assert(entry->file_size == st.st_size);
#ifndef _WIN32
		/*
		 * Windows doesn't populate these fields, and the signage is
		 * wrong in the Windows version of the struct, so lets avoid
		 * the "comparing signed and unsigned" compilation warning in
		 * that case.
		 */
		cl_assert(entry->uid == st.st_uid);
		cl_assert(entry->gid == st.st_gid);
#endif
	}

	/*
	 * Build the tree from the index
	 */
	cl_git_pass(git_index_write_tree(&tree_oid, index));

	cl_assert(git_oid_cmp(&expected_tree_oid, &tree_oid) == 0);

	/*
	 * Commit the staged file
	 */
	cl_git_pass(git_signature_new(&signature, "nulltoken", "emeric.fermas@gmail.com", 1323847743, 60));
	cl_git_pass(git_tree_lookup(&tree, repo, &tree_oid));

	cl_assert_equal_i(16, git_message_prettify(buffer, 128, "Initial commit", 0));

	cl_git_pass(git_commit_create_v(
		&commit_oid,
		repo,
		"HEAD",
		signature,
		signature,
		NULL,
		buffer,
		tree,
		0));

	cl_assert(git_oid_cmp(&expected_commit_oid, &commit_oid) == 0);

	git_signature_free(signature);
	git_tree_free(tree);
	git_index_free(index);
}
