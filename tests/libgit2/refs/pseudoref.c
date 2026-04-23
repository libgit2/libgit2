#include "clar_libgit2.h"

#include "futils.h"
#include "refs.h"

void test_refs_pseudoref__cleanup(void)
{
	git_futils_rmdir_r("dst.git", NULL, GIT_RMDIR_REMOVE_FILES);
	cl_git_sandbox_cleanup();
}

void test_refs_pseudoref__lookup_fetch_head_after_fetch(void)
{
	git_repository *dst;
	git_remote *remote;
	git_reference *ref;
	git_object *commit;
	git_str url = GIT_STR_INIT;

	cl_git_sandbox_init("testrepo.git");

	cl_git_pass(git_repository_init(&dst, "dst.git", true));

	cl_git_pass(git_str_printf(&url, "%s", cl_git_path_url("testrepo.git")));
	cl_git_pass(git_remote_create_anonymous(&remote, dst, url.ptr));
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	git_remote_free(remote);
	git_str_dispose(&url);

	cl_git_pass(git_reference_lookup(&ref, dst, "FETCH_HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_DIRECT, git_reference_type(ref));
	cl_assert(ref->db != NULL);
	cl_git_pass(git_reference_peel(&commit, ref, GIT_OBJECT_COMMIT));

	git_object_free(commit);
	git_reference_free(ref);
	git_repository_free(dst);
}
