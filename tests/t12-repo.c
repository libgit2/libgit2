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

git_odb_backend *new_backend(int position)
{
	fake_backend *b;

	b = git__malloc(sizeof(fake_backend));
	if (b == NULL)
		return NULL;

	memset(b, 0x0, sizeof(fake_backend));
	b->position = position;
	return (git_odb_backend *)b;
}

int test_backend_sorting(git_odb *odb)
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

	if (gitfo_isdir(working_directory) == GIT_SUCCESS)
		return GIT_ERROR;

	git__joinpath(path_odb, expected_path_repository, GIT_OBJECTS_DIR);

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
	}

	git_repository_free(repo);
	rmdir_recurs(working_directory);

	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	rmdir_recurs(working_directory);
	return GIT_ERROR;
}

BEGIN_TEST(init0, "initialize a standard repo")
	char path_index[GIT_PATH_MAX], path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, TEMP_REPO_FOLDER, GIT_DIR);
	git__joinpath(path_index, path_repository, GIT_INDEX_FILE);

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
END_TEST

BEGIN_TEST(init1, "initialize a bare repo")
	char path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, TEMP_REPO_FOLDER, "");

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, BARE_REPOSITORY, NULL, path_repository, NULL));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, BARE_REPOSITORY, NULL, path_repository, NULL));
END_TEST

BEGIN_TEST(init2, "Initialize and open a bare repo with a relative path escaping out of the current working directory")
	char path_repository[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	const int mode = 0755; /* or 0777 ? */
	git_repository* repo;

	must_pass(gitfo_getcwd(current_workdir, sizeof(current_workdir)));

	git__joinpath(path_repository, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(gitfo_mkdir_recurs(path_repository, mode));

	must_pass(chdir(path_repository));

	must_pass(git_repository_init(&repo, "../d/e.git", 1));
	must_pass(git__suffixcmp(repo->path_repository, "/a/b/d/e.git/"));

	git_repository_free(repo);

	must_pass(git_repository_open(&repo, "../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	rmdir_recurs(TEMP_REPO_FOLDER);
END_TEST

#define EMPTY_BARE_REPOSITORY_NAME		"empty_bare.git"
#define EMPTY_BARE_REPOSITORY_FOLDER	TEST_RESOURCES "/" EMPTY_BARE_REPOSITORY_NAME "/"

BEGIN_TEST(open0, "Open a bare repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_BARE_REPOSITORY_FOLDER, TEMP_REPO_FOLDER));
	must_pass(remove_placeholders(TEMP_REPO_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, TEMP_REPO_FOLDER));
	must_be_true(git_repository_path(repo) != NULL);
	must_be_true(git_repository_workdir(repo) == NULL);

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_REPO_FOLDER));
END_TEST

#define SOURCE_EMPTY_REPOSITORY_NAME	"empty_standard_repo/.gitted"
#define EMPTY_REPOSITORY_NAME			"empty_standard_repo/.git"
#define EMPTY_REPOSITORY_FOLDER			TEST_RESOURCES "/" SOURCE_EMPTY_REPOSITORY_NAME "/"
#define DEST_REPOSITORY_FOLDER			TEMP_REPO_FOLDER DOT_GIT "/"

BEGIN_TEST(open1, "Open a standard repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_REPOSITORY_FOLDER, DEST_REPOSITORY_FOLDER));
	must_pass(remove_placeholders(DEST_REPOSITORY_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, DEST_REPOSITORY_FOLDER));
	must_be_true(git_repository_path(repo) != NULL);
	must_be_true(git_repository_workdir(repo) != NULL);

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_REPO_FOLDER));
END_TEST


BEGIN_TEST(open2, "Open a bare repository with a relative path escaping out of the current working directory")
	char new_current_workdir[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	char path_repository[GIT_PATH_MAX];

	const int mode = 0755; /* or 0777 ? */
	git_repository* repo;

	/* Setup the repository to open */
	must_pass(gitfo_getcwd(current_workdir, sizeof(current_workdir)));
	strcpy(path_repository, current_workdir);
	git__joinpath_n(path_repository, 3, path_repository, TEMP_REPO_FOLDER, "a/d/e.git");
	must_pass(copydir_recurs(REPOSITORY_FOLDER, path_repository));

	/* Change the current working directory */
	git__joinpath(new_current_workdir, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(gitfo_mkdir_recurs(new_current_workdir, mode));
	must_pass(chdir(new_current_workdir));

	must_pass(git_repository_open(&repo, "../../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	rmdir_recurs(TEMP_REPO_FOLDER);
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
END_SUITE

