#include "clar_libgit2.h"
#include "patch.h"
#include "patch_parse.h"

#include "patch_common.h"

void test_patch_parse__original_to_change_middle(void)
{
	git_patch *patch;
	const git_diff_delta *delta;
	char idstr[GIT_OID_HEXSZ+1] = {0};

	cl_git_pass(git_patch_from_buffer(
		&patch, PATCH_ORIGINAL_TO_CHANGE_MIDDLE,
		strlen(PATCH_ORIGINAL_TO_CHANGE_MIDDLE), NULL));

	cl_assert((delta = git_patch_get_delta(patch)) != NULL);
	cl_assert_equal_i(2, delta->nfiles);

	cl_assert_equal_s(delta->old_file.path, "file.txt");
	cl_assert(delta->old_file.mode == GIT_FILEMODE_BLOB);
	cl_assert_equal_i(7, delta->old_file.id_abbrev);
	git_oid_nfmt(idstr, delta->old_file.id_abbrev, &delta->old_file.id);
	cl_assert_equal_s(idstr, "9432026");
	cl_assert_equal_i(0, delta->old_file.size);

	cl_assert_equal_s(delta->new_file.path, "file.txt");
	cl_assert(delta->new_file.mode == GIT_FILEMODE_BLOB);
	cl_assert_equal_i(7, delta->new_file.id_abbrev);
	git_oid_nfmt(idstr, delta->new_file.id_abbrev, &delta->new_file.id);
	cl_assert_equal_s(idstr, "cd8fd12");
	cl_assert_equal_i(0, delta->new_file.size);

	git_patch_free(patch);
}
