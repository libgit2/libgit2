#include "clay_libgit2.h"
#include "fileops.h"

static const char *empty_tmp_dir = "test_gitfo_rmdir_recurs_test";

void test_rmdir__initialize()
{
	char path[GIT_PATH_MAX];

	must_pass(p_mkdir(empty_tmp_dir, 0755));

	git_path_join(path, empty_tmp_dir, "/one");
	must_pass(p_mkdir(path, 0755));

	git_path_join(path, empty_tmp_dir, "/one/two_one");
	must_pass(p_mkdir(path, 0755));

	git_path_join(path, empty_tmp_dir, "/one/two_two");
	must_pass(p_mkdir(path, 0755));

	git_path_join(path, empty_tmp_dir, "/one/two_two/three");
	must_pass(p_mkdir(path, 0755));

	git_path_join(path, empty_tmp_dir, "/two");
	must_pass(p_mkdir(path, 0755));
}

/* make sure empty dir can be deleted recusively */
void test_rmdir__delete_recursive(void)
{
	must_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
}

/* make sure non-empty dir cannot be deleted recusively */
void test_rmdir__fail_to_delete_non_empty_dir(void)
{
	char file[GIT_PATH_MAX];
	int fd;

	git_path_join(file, empty_tmp_dir, "/two/file.txt");

	fd = p_creat(file, 0755);
	must_be_true(fd >= 0);

	must_pass(p_close(fd));
	must_fail(git_futils_rmdir_r(empty_tmp_dir, 0));
	must_pass(p_unlink(file));
	must_pass(git_futils_rmdir_r(empty_tmp_dir, 0));
}
