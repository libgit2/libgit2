/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include "odb.h"
#include "git2/odb_backend.h"
#include "repository.h"

#define EMPTY_BARE_REPOSITORY_FOLDER TEST_RESOURCES "/empty_bare.git/"

#define DISCOVER_FOLDER TEMP_REPO_FOLDER "discover.git"

#define SUB_REPOSITORY_FOLDER_NAME "sub_repo"
#define SUB_REPOSITORY_FOLDER DISCOVER_FOLDER "/" SUB_REPOSITORY_FOLDER_NAME
#define SUB_REPOSITORY_FOLDER_SUB SUB_REPOSITORY_FOLDER "/sub"
#define SUB_REPOSITORY_FOLDER_SUB_SUB SUB_REPOSITORY_FOLDER_SUB "/subsub"
#define SUB_REPOSITORY_FOLDER_SUB_SUB_SUB SUB_REPOSITORY_FOLDER_SUB_SUB "/subsubsub"

#define REPOSITORY_ALTERNATE_FOLDER DISCOVER_FOLDER "/alternate_sub_repo"
#define REPOSITORY_ALTERNATE_FOLDER_SUB REPOSITORY_ALTERNATE_FOLDER "/sub"
#define REPOSITORY_ALTERNATE_FOLDER_SUB_SUB REPOSITORY_ALTERNATE_FOLDER_SUB "/subsub"
#define REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB REPOSITORY_ALTERNATE_FOLDER_SUB_SUB "/subsubsub"

#define ALTERNATE_MALFORMED_FOLDER1 DISCOVER_FOLDER "/alternate_malformed_repo1"
#define ALTERNATE_MALFORMED_FOLDER2 DISCOVER_FOLDER "/alternate_malformed_repo2"
#define ALTERNATE_MALFORMED_FOLDER3 DISCOVER_FOLDER "/alternate_malformed_repo3"
#define ALTERNATE_NOT_FOUND_FOLDER DISCOVER_FOLDER "/alternate_not_found_repo"

static int ensure_repository_discover(const char *start_path, const char *ceiling_dirs, const char *expected_path)
{
	int error;
	char found_path[GIT_PATH_MAX];

	error = git_repository_discover(found_path, sizeof(found_path), start_path, 0, ceiling_dirs);
	//across_fs is always 0 as we can't automate the filesystem change tests

	if (error < GIT_SUCCESS)
		return error;

	return strcmp(found_path, expected_path) ? GIT_ERROR : GIT_SUCCESS;
}

static int write_file(const char *path, const char *content)
{
	int error;
	git_file file;

	if (git_futils_exists(path) == GIT_SUCCESS) {
		error = p_unlink(path);

		if (error < GIT_SUCCESS)
			return error;
	}

	file = git_futils_creat_withpath(path, 0777, 0666);
	if (file < GIT_SUCCESS)
		return file;

	error = p_write(file, content, strlen(content) * sizeof(char));

	p_close(file);

	return error;
}

//no check is performed on ceiling_dirs length, so be sure it's long enough
static int append_ceiling_dir(git_buf *ceiling_dirs, const char *path)
{
	git_buf pretty_path = GIT_BUF_INIT;
	int error;
	char ceiling_separator[2] = { GIT_PATH_LIST_SEPARATOR, '\0' };

	error = git_path_prettify_dir(&pretty_path, path, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to append ceiling directory.");

	if (ceiling_dirs->size > 0)
		git_buf_puts(ceiling_dirs, ceiling_separator);
	git_buf_puts(ceiling_dirs, pretty_path.ptr);

	git_buf_free(&pretty_path);

	return git_buf_lasterror(ceiling_dirs);
}

BEGIN_TEST(discover0, "test discover")
	git_repository *repo;
	git_buf ceiling_dirs_buf = GIT_BUF_INIT;
	const char *ceiling_dirs;
	char repository_path[GIT_PATH_MAX];
	char sub_repository_path[GIT_PATH_MAX];
	char found_path[GIT_PATH_MAX];
	const mode_t mode = 0777;

	git_futils_mkdir_r(DISCOVER_FOLDER, NULL, mode);
	must_pass(append_ceiling_dir(&ceiling_dirs_buf, TEMP_REPO_FOLDER));
	ceiling_dirs = git_buf_cstr(&ceiling_dirs_buf);

	must_be_true(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs) == GIT_ENOTAREPO);

	must_pass(git_repository_init(&repo, DISCOVER_FOLDER, 1));
	must_pass(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs));
	git_repository_free(repo);

	must_pass(git_repository_init(&repo, SUB_REPOSITORY_FOLDER, 0));
	must_pass(git_futils_mkdir_r(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, NULL, mode));
	must_pass(git_repository_discover(sub_repository_path, sizeof(sub_repository_path), SUB_REPOSITORY_FOLDER, 0, ceiling_dirs));

	must_pass(git_futils_mkdir_r(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, NULL, mode));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, ceiling_dirs, sub_repository_path));

	must_pass(git_futils_mkdir_r(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, NULL, mode));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER "/" DOT_GIT, "gitdir: ../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB "/" DOT_GIT, "gitdir: ../../../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB "/" DOT_GIT, "gitdir: ../../../../"));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, ceiling_dirs, repository_path));

	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER1, NULL, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER1 "/" DOT_GIT, "Anything but not gitdir:"));
	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER2, NULL, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER2 "/" DOT_GIT, "gitdir:"));
	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER3, NULL, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER3 "/" DOT_GIT, "gitdir: \n\n\n"));
	must_pass(git_futils_mkdir_r(ALTERNATE_NOT_FOUND_FOLDER, NULL, mode));
	must_pass(write_file(ALTERNATE_NOT_FOUND_FOLDER "/" DOT_GIT, "gitdir: a_repository_that_surely_does_not_exist"));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER1, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER2, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER3, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_NOT_FOUND_FOLDER, 0, ceiling_dirs));

	must_pass(append_ceiling_dir(&ceiling_dirs_buf, SUB_REPOSITORY_FOLDER));
	ceiling_dirs = git_buf_cstr(&ceiling_dirs_buf);

	//this must pass as ceiling_directories cannot predent the current
	//working directory to be checked
	must_pass(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB_SUB, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, 0, ceiling_dirs));

	//.gitfile redirection should not be affected by ceiling directories
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, ceiling_dirs, repository_path));

	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
	git_repository_free(repo);
	git_buf_free(&ceiling_dirs_buf);
END_TEST

BEGIN_SUITE(repository)
	ADD_TEST(discover0);
END_SUITE

