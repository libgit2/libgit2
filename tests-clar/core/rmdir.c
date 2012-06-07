#include "clar_libgit2.h"
#include "fileops.h"

static const char *empty_tmp_dir = "test_gitfo_rmdir_recurs_test";

void test_core_rmdir__initialize(void)
{
	git_buf path = GIT_BUF_INIT;

	cl_must_pass(p_mkdir(empty_tmp_dir, 0777));

	cl_git_pass(git_buf_joinpath(&path, empty_tmp_dir, "/one"));
	cl_must_pass(p_mkdir(path.ptr, 0777));

	cl_git_pass(git_buf_joinpath(&path, empty_tmp_dir, "/one/two_one"));
	cl_must_pass(p_mkdir(path.ptr, 0777));

	cl_git_pass(git_buf_joinpath(&path, empty_tmp_dir, "/one/two_two"));
	cl_must_pass(p_mkdir(path.ptr, 0777));

	cl_git_pass(git_buf_joinpath(&path, empty_tmp_dir, "/one/two_two/three"));
	cl_must_pass(p_mkdir(path.ptr, 0777));

	cl_git_pass(git_buf_joinpath(&path, empty_tmp_dir, "/two"));
	cl_must_pass(p_mkdir(path.ptr, 0777));

	git_buf_free(&path);
}

/* make sure empty dir can be deleted recusively */
void test_core_rmdir__delete_recursive(void)
{
	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, GIT_DIRREMOVAL_EMPTY_HIERARCHY));
}

/* make sure non-empty dir cannot be deleted recusively */
void test_core_rmdir__fail_to_delete_non_empty_dir(void)
{
	git_buf file = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&file, empty_tmp_dir, "/two/file.txt"));

	cl_git_mkfile(git_buf_cstr(&file), "dummy");

	cl_git_fail(git_futils_rmdir_r(empty_tmp_dir, GIT_DIRREMOVAL_EMPTY_HIERARCHY));

	cl_must_pass(p_unlink(file.ptr));
	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, GIT_DIRREMOVAL_EMPTY_HIERARCHY));

	git_buf_free(&file);
}

void test_core_rmdir__can_skip__non_empty_dir(void)
{
	git_buf file = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&file, empty_tmp_dir, "/two/file.txt"));

	cl_git_mkfile(git_buf_cstr(&file), "dummy");

	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, GIT_DIRREMOVAL_ONLY_EMPTY_DIRS));
	cl_assert(git_path_exists(git_buf_cstr(&file)) == true);

	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, GIT_DIRREMOVAL_FILES_AND_DIRS));
	cl_assert(git_path_exists(empty_tmp_dir) == false);

	git_buf_free(&file);
}
