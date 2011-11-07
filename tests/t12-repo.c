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

typedef struct {
	git_odb_backend base;
	int position;
} fake_backend;

static git_odb_backend *new_backend(int position)
{
	fake_backend *b;

	b = git__malloc(sizeof(fake_backend));
	if (b == NULL)
		return NULL;

	memset(b, 0x0, sizeof(fake_backend));
	b->position = position;
	return (git_odb_backend *)b;
}

static int test_backend_sorting(git_odb *odb)
{
	unsigned int i;

	for (i = 0; i < odb->backends.length; ++i) {
		fake_backend *internal = *((fake_backend **)git_vector_get(&odb->backends, i));

		if (internal == NULL)
			return GIT_ERROR;

		if (internal->position != (int)i)
			return GIT_ERROR;
	}

	return GIT_SUCCESS;
}

BEGIN_TEST(odb0, "assure that ODB backends are properly sorted")
	git_odb *odb;
	must_pass(git_odb_new(&odb));
	must_pass(git_odb_add_backend(odb, new_backend(0), 5));
	must_pass(git_odb_add_backend(odb, new_backend(2), 3));
	must_pass(git_odb_add_backend(odb, new_backend(1), 4));
	must_pass(git_odb_add_backend(odb, new_backend(3), 1));
	must_pass(test_backend_sorting(odb));
	git_odb_close(odb);
END_TEST

BEGIN_TEST(odb1, "assure that alternate backends are properly sorted")
	git_odb *odb;
	must_pass(git_odb_new(&odb));
	must_pass(git_odb_add_backend(odb, new_backend(0), 5));
	must_pass(git_odb_add_backend(odb, new_backend(2), 3));
	must_pass(git_odb_add_backend(odb, new_backend(1), 4));
	must_pass(git_odb_add_backend(odb, new_backend(3), 1));
	must_pass(git_odb_add_alternate(odb, new_backend(4), 5));
	must_pass(git_odb_add_alternate(odb, new_backend(6), 3));
	must_pass(git_odb_add_alternate(odb, new_backend(5), 4));
	must_pass(git_odb_add_alternate(odb, new_backend(7), 1));
	must_pass(test_backend_sorting(odb));
	git_odb_close(odb);
END_TEST


#define STANDARD_REPOSITORY 0
#define BARE_REPOSITORY 1

static int ensure_repository_init(
	const char *working_directory,
	int repository_kind,
	const char *expected_path_index,
	const char *expected_path_repository,
	const char *expected_working_directory)
{
	char path_odb[GIT_PATH_MAX];
	git_repository *repo;

	if (git_futils_isdir(working_directory) == GIT_SUCCESS)
		return GIT_ERROR;

	git_path_join(path_odb, expected_path_repository, GIT_OBJECTS_DIR);

	if (git_repository_init(&repo, working_directory, repository_kind) < GIT_SUCCESS)
		return GIT_ERROR;

	if (repo->path_workdir != NULL || expected_working_directory != NULL) {
		if (git__suffixcmp(repo->path_workdir, expected_working_directory) != 0)
			goto cleanup;
	}

	if (git__suffixcmp(repo->path_odb, path_odb) != 0)
		goto cleanup;

	if (git__suffixcmp(repo->path_repository, expected_path_repository) != 0)
		goto cleanup;

	if (repo->path_index != NULL || expected_path_index != NULL) {
		if (git__suffixcmp(repo->path_index, expected_path_index) != 0)
			goto cleanup;

#ifdef GIT_WIN32
		if ((GetFileAttributes(repo->path_repository) & FILE_ATTRIBUTE_HIDDEN) == 0)
			goto cleanup;
#endif

		if (git_repository_is_bare(repo) == 1)
			goto cleanup;
	} else if (git_repository_is_bare(repo) == 0)
			goto cleanup;

	if (git_repository_is_empty(repo) == 0)
		goto cleanup;

	git_repository_free(repo);
	git_futils_rmdir_r(working_directory, 1);

	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	git_futils_rmdir_r(working_directory, 1);
	return GIT_ERROR;
}

BEGIN_TEST(init0, "initialize a standard repo")
	char path_index[GIT_PATH_MAX], path_repository[GIT_PATH_MAX];

	git_path_join(path_repository, TEMP_REPO_FOLDER, GIT_DIR);
	git_path_join(path_index, path_repository, GIT_INDEX_FILE);

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
END_TEST

BEGIN_TEST(init1, "initialize a bare repo")
	char path_repository[GIT_PATH_MAX];

	git_path_join(path_repository, TEMP_REPO_FOLDER, "");

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, BARE_REPOSITORY, NULL, path_repository, NULL));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, BARE_REPOSITORY, NULL, path_repository, NULL));
END_TEST

BEGIN_TEST(init2, "Initialize and open a bare repo with a relative path escaping out of the current working directory")
	char path_repository[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	const mode_t mode = 0777;
	git_repository* repo;

	must_pass(p_getcwd(current_workdir, sizeof(current_workdir)));

	git_path_join(path_repository, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(git_futils_mkdir_r(path_repository, mode));

	must_pass(chdir(path_repository));

	must_pass(git_repository_init(&repo, "../d/e.git", 1));
	must_pass(git__suffixcmp(repo->path_repository, "/a/b/d/e.git/"));

	git_repository_free(repo);

	must_pass(git_repository_open(&repo, "../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST

#define EMPTY_BARE_REPOSITORY_FOLDER TEST_RESOURCES "/empty_bare.git/"

BEGIN_TEST(open0, "Open a bare repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_BARE_REPOSITORY_FOLDER, TEMP_REPO_FOLDER));
	must_pass(remove_placeholders(TEMP_REPO_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, TEMP_REPO_FOLDER));
	must_be_true(git_repository_path(repo, GIT_REPO_PATH) != NULL);
	must_be_true(git_repository_path(repo, GIT_REPO_PATH_WORKDIR) == NULL);

	git_repository_free(repo);
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST

BEGIN_TEST(open1, "Open a standard repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_REPOSITORY_FOLDER, TEST_STD_REPO_FOLDER));
	must_pass(remove_placeholders(TEST_STD_REPO_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, TEST_STD_REPO_FOLDER));
	must_be_true(git_repository_path(repo, GIT_REPO_PATH) != NULL);
	must_be_true(git_repository_path(repo, GIT_REPO_PATH_WORKDIR) != NULL);

	git_repository_free(repo);
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST


BEGIN_TEST(open2, "Open a bare repository with a relative path escaping out of the current working directory")
	char new_current_workdir[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	char path_repository[GIT_PATH_MAX];

	const mode_t mode = 0777;
	git_repository* repo;

	/* Setup the repository to open */
	must_pass(p_getcwd(current_workdir, sizeof(current_workdir)));
	strcpy(path_repository, current_workdir);
	git_path_join_n(path_repository, 3, path_repository, TEMP_REPO_FOLDER, "a/d/e.git");
	must_pass(copydir_recurs(REPOSITORY_FOLDER, path_repository));

	/* Change the current working directory */
	git_path_join(new_current_workdir, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(git_futils_mkdir_r(new_current_workdir, mode));
	must_pass(chdir(new_current_workdir));

	must_pass(git_repository_open(&repo, "../../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	must_pass(git_futils_rmdir_r(TEMP_REPO_FOLDER, 1));
END_TEST

BEGIN_TEST(empty0, "test if a repository is empty or not")

	git_repository *repo_empty, *repo_normal;

	must_pass(git_repository_open(&repo_normal, REPOSITORY_FOLDER));
	must_be_true(git_repository_is_empty(repo_normal) == 0);
	git_repository_free(repo_normal);

	must_pass(git_repository_open(&repo_empty, EMPTY_BARE_REPOSITORY_FOLDER));
	must_be_true(git_repository_is_empty(repo_empty) == 1);
	git_repository_free(repo_empty);
END_TEST

BEGIN_TEST(detached0, "test if HEAD is detached")
	git_repository *repo;
	git_reference *ref;
	git_oid oid;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_be_true(git_repository_head_detached(repo) == 0);

	/* detach the HEAD */
	git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd");
	must_pass(git_reference_create_oid(&ref, repo, "HEAD", &oid, 1));
	must_be_true(git_repository_head_detached(repo) == 1);

	/* take the reop back to it's original state */
	must_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	must_be_true(git_repository_head_detached(repo) == 0);

	git_repository_free(repo);

	git_reference_free(ref);
END_TEST

BEGIN_TEST(orphan0, "test if HEAD is orphan")
	git_repository *repo;
	git_reference *ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_be_true(git_repository_head_orphan(repo) == 0);

	/* orphan HEAD */
	must_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/orphan", 1));
	must_be_true(git_repository_head_orphan(repo) == 1);

	/* take the reop back to it's original state */
	must_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	must_be_true(git_repository_head_orphan(repo) == 0);

	git_repository_free(repo);

	git_reference_free(ref);
END_TEST

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
static int append_ceiling_dir(char *ceiling_dirs, const char *path)
{
	int len = strlen(ceiling_dirs);
	int error;

	error = git_path_prettify_dir(ceiling_dirs + len + (len ? 1 : 0), path, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to append ceiling directory.");

	if (len)
		ceiling_dirs[len] = GIT_PATH_LIST_SEPARATOR;

	return GIT_SUCCESS;
}

BEGIN_TEST(discover0, "test discover")
	git_repository *repo;
	char ceiling_dirs[GIT_PATH_MAX * 2] = "";
	char repository_path[GIT_PATH_MAX];
	char sub_repository_path[GIT_PATH_MAX];
	char found_path[GIT_PATH_MAX];
	const mode_t mode = 0777;

	git_futils_mkdir_r(DISCOVER_FOLDER, mode);
	must_pass(append_ceiling_dir(ceiling_dirs, TEMP_REPO_FOLDER));

	must_be_true(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs) == GIT_ENOTAREPO);

	must_pass(git_repository_init(&repo, DISCOVER_FOLDER, 1));
	must_pass(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs));
	git_repository_free(repo);

	must_pass(git_repository_init(&repo, SUB_REPOSITORY_FOLDER, 0));
	must_pass(git_futils_mkdir_r(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, mode));
	must_pass(git_repository_discover(sub_repository_path, sizeof(sub_repository_path), SUB_REPOSITORY_FOLDER, 0, ceiling_dirs));

	must_pass(git_futils_mkdir_r(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, mode));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, ceiling_dirs, sub_repository_path));

	must_pass(git_futils_mkdir_r(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, mode));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER "/" DOT_GIT, "gitdir: ../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB "/" DOT_GIT, "gitdir: ../../../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB "/" DOT_GIT, "gitdir: ../../../../"));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, ceiling_dirs, repository_path));

	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER1, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER1 "/" DOT_GIT, "Anything but not gitdir:"));
	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER2, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER2 "/" DOT_GIT, "gitdir:"));
	must_pass(git_futils_mkdir_r(ALTERNATE_MALFORMED_FOLDER3, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER3 "/" DOT_GIT, "gitdir: \n\n\n"));
	must_pass(git_futils_mkdir_r(ALTERNATE_NOT_FOUND_FOLDER, mode));
	must_pass(write_file(ALTERNATE_NOT_FOUND_FOLDER "/" DOT_GIT, "gitdir: a_repository_that_surely_does_not_exist"));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER1, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER2, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER3, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_NOT_FOUND_FOLDER, 0, ceiling_dirs));

	must_pass(append_ceiling_dir(ceiling_dirs, SUB_REPOSITORY_FOLDER));
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
END_TEST

BEGIN_SUITE(repository)
	ADD_TEST(odb0);
	ADD_TEST(odb1);
	ADD_TEST(init0);
	ADD_TEST(init1);
	ADD_TEST(init2);
	ADD_TEST(open0);
	ADD_TEST(open1);
	ADD_TEST(open2);
	ADD_TEST(empty0);
	ADD_TEST(detached0);
	ADD_TEST(orphan0);
	ADD_TEST(discover0);
END_SUITE

