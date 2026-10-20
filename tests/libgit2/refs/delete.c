#include "clar_libgit2.h"

#include "futils.h"
#include "git2/reflog.h"
#include "git2/refdb.h"
#include "reflog.h"
#include "ref_helpers.h"

static const char *packed_test_head_name = "refs/heads/packed-test";
static const char *current_master_tip = "a65fedf39aefe402d3bb6e24df4d4f5fe4547750";

static git_repository *g_repo;



void test_refs_delete__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_refs_delete__cleanup(void)
{
   cl_git_sandbox_cleanup();
}



void test_refs_delete__packed_loose(void)
{
	/* deleting a ref which is both packed and loose should remove both tracks in the filesystem */
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_str temp_path = GIT_STR_INIT;

	/* Ensure the loose reference exists on the file system */
	cl_git_pass(git_str_joinpath(&temp_path, git_repository_path(g_repo), packed_test_head_name));
	if (cl_repo_has_ref_format(g_repo, "files"))
		cl_assert(git_fs_path_exists(temp_path.ptr));

	/* Lookup the reference */
	cl_git_pass(git_reference_lookup(&looked_up_ref, g_repo, packed_test_head_name));

	/* Ensure it's the loose version that has been found */
	if (cl_repo_has_ref_format(g_repo, "files"))
		cl_assert(reference_is_packed(looked_up_ref) == 0);

	/* Now that the reference is deleted... */
	cl_git_pass(git_reference_delete(looked_up_ref));
	git_reference_free(looked_up_ref);

	/* Looking up the reference once again should not retrieve it */
	cl_git_fail(git_reference_lookup(&another_looked_up_ref, g_repo, packed_test_head_name));

	/* Ensure the loose reference doesn't exist any longer on the file system */
	if (cl_repo_has_ref_format(g_repo, "files"))
		cl_assert(!git_fs_path_exists(temp_path.ptr));

	git_reference_free(another_looked_up_ref);
	git_str_dispose(&temp_path);
}

void test_refs_delete__packed_only(void)
{
	/* can delete a just packed reference */
	git_reference *ref;
	git_refdb *refdb;
	git_oid id;
	const char *new_ref = "refs/heads/new_ref";

	git_oid_from_string(&id, current_master_tip, GIT_OID_SHA1);

	/* Create and write the new object id reference */
	cl_git_pass(git_reference_create(&ref, g_repo, new_ref, &id, 0, NULL));
	git_reference_free(ref);

	/* Lookup the reference */
	cl_git_pass(git_reference_lookup(&ref, g_repo, new_ref));

	/* Ensure it's a loose reference */
	if (cl_repo_has_ref_format(g_repo, "files"))
		cl_assert(reference_is_packed(ref) == 0);

	/* Pack all existing references */
	cl_git_pass(git_repository_refdb(&refdb, g_repo));
	cl_git_pass(git_refdb_compress(refdb));

	/* Reload the reference from disk */
	git_reference_free(ref);
	cl_git_pass(git_reference_lookup(&ref, g_repo, new_ref));

	/* Ensure it's a packed reference */
	if (cl_repo_has_ref_format(g_repo, "files"))
		cl_assert(reference_is_packed(ref) == 1);

	/* This should pass */
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);
	git_refdb_free(refdb);
}

void test_refs_delete__remove(void)
{
	git_reference *ref;

	/* Check that passing no old values lets us delete */

	cl_git_pass(git_reference_lookup(&ref, g_repo, packed_test_head_name));
	git_reference_free(ref);

	cl_git_pass(git_reference_remove(g_repo, packed_test_head_name));

	cl_git_fail(git_reference_lookup(&ref, g_repo, packed_test_head_name));
}

void test_refs_delete__head(void)
{
	git_reference *ref;

	/* Check that it is not possible to delete HEAD */

	cl_git_pass(git_reference_lookup(&ref, g_repo, "HEAD"));
	cl_git_fail(git_reference_delete(ref));
	git_reference_free(ref);
}

void test_refs_delete__remove_head(void)
{
	git_reference *ref;
	git_str head_path = GIT_STR_INIT;

	/* Check that it is not possible to delete HEAD by name either */

	cl_git_pass(git_str_joinpath(&head_path, git_repository_path(g_repo), "HEAD"));
	cl_assert(git_fs_path_exists(head_path.ptr));

	cl_git_fail(git_reference_remove(g_repo, "HEAD"));

	cl_assert(git_fs_path_exists(head_path.ptr));
	cl_git_pass(git_reference_lookup(&ref, g_repo, "HEAD"));
	git_reference_free(ref);

	git_str_dispose(&head_path);
}

void test_refs_delete__remove_invalid_name(void)
{
	/*
	 * Names that are not valid reference names must be rejected before
	 * they reach the backend; otherwise a caller passing an untrusted
	 * name can remove files from the repository control directory.
	 */
	const char *invalid[] = {
		"config",
		"description",
		"index",
		"refs/heads/ab..cd",
		"refs/heads/foo.lock",
		"refs/heads/with space",
		"refs/heads/"
	};
	git_str path = GIT_STR_INIT;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(invalid); i++) {
		cl_git_fail_with(GIT_EINVALIDSPEC,
			git_reference_remove(g_repo, invalid[i]));
	}

	/* the control files that exist must still be there */
	cl_git_pass(git_str_joinpath(&path, git_repository_path(g_repo), "config"));
	cl_assert(git_fs_path_exists(path.ptr));

	cl_git_pass(git_str_joinpath(&path, git_repository_path(g_repo), "index"));
	cl_assert(git_fs_path_exists(path.ptr));

	git_str_dispose(&path);
}

void test_refs_delete__remove_one_level_name(void)
{
	/* one-level names that _are_ valid references remain removable */
	git_reference *ref;
	git_oid id;

	git_oid_from_string(&id, current_master_tip, GIT_OID_SHA1);

	cl_git_pass(git_reference_create(&ref, g_repo, "ORIG_HEAD", &id, 0, NULL));
	git_reference_free(ref);

	cl_git_pass(git_reference_remove(g_repo, "ORIG_HEAD"));
	cl_git_fail(git_reference_lookup(&ref, g_repo, "ORIG_HEAD"));
}
