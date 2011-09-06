#include "clay_libgit2.h"
#include "fileops.h"

static const char *empty_tmp_dir = "test_gitfo_rmdir_recurs_test";

void test_core_rmdir__initialize(void)
{
	char path[GIT_PATH_MAX];

	cl_must_pass(p_mkdir(empty_tmp_dir, 0777));

	git_path_join(path, empty_tmp_dir, "/one");
	cl_must_pass(p_mkdir(path, 0777));

	git_path_join(path, empty_tmp_dir, "/one/two_one");
	cl_must_pass(p_mkdir(path, 0777));

	git_path_join(path, empty_tmp_dir, "/one/two_two");
	cl_must_pass(p_mkdir(path, 0777));

	git_path_join(path, empty_tmp_dir, "/one/two_two/three");
	cl_must_pass(p_mkdir(path, 0777));

	git_path_join(path, empty_tmp_dir, "/two");
	cl_must_pass(p_mkdir(path, 0777));
}

/* make sure empty dir can be deleted recusively */
void test_core_rmdir__delete_recursive(void)
{
	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
}

/* make sure non-empty dir cannot be deleted recusively */
void test_core_rmdir__fail_to_delete_non_empty_dir(void)
{
	char file[GIT_PATH_MAX];
	int fd;

	git_path_join(file, empty_tmp_dir, "/two/file.txt");

	fd = p_creat(file, 0666);
	cl_assert(fd >= 0);

	cl_must_pass(p_close(fd));
	cl_git_fail(git_futils_rmdir_r(empty_tmp_dir, 0));

	cl_must_pass(p_unlink(file));
	cl_git_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
}
