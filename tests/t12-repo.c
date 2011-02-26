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

BEGIN_TEST("odb", backend_sorting)
	git_odb *odb;
	must_pass(git_odb_new(&odb));
	must_pass(git_odb_add_backend(odb, new_backend(0), 5));
	must_pass(git_odb_add_backend(odb, new_backend(2), 3));
	must_pass(git_odb_add_backend(odb, new_backend(1), 4));
	must_pass(git_odb_add_backend(odb, new_backend(3), 1));
	must_pass(test_backend_sorting(odb));
	git_odb_close(odb);
END_TEST

BEGIN_TEST("odb", backend_alternates_sorting)
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

#define WORK_TREE_WITHOUT_TRAILING_SLASH TEST_RESOURCES "/temp_working"
#define WORK_TREE_WITH_TRAILING_SLASH WORK_TREE_WITHOUT_TRAILING_SLASH "/"

#define STANDARD_REPOSITORY 0
#define BARE_REPOSITORY 1

static void ensure_repository_init(git_test *_gittest, char *working_directory, int repository_kind, char *expected_path_index, char *expected_path_repository, char *expected_working_directory)
{
	char path_odb[GIT_PATH_MAX];
	git_repository *repo;

	must_be_true(gitfo_isdir(working_directory));

	git__joinpath(path_odb, expected_path_repository, GIT_OBJECTS_DIR);

	must_pass(git_repository_init(&repo, working_directory, repository_kind));
	must_be_true((repo->path_workdir == NULL && expected_working_directory == NULL) || !strcmp(repo->path_workdir, expected_working_directory));
	must_be_true(!strcmp(repo->path_odb, path_odb));
	must_be_true(!strcmp(repo->path_repository, expected_path_repository));
	must_be_true((repo->path_index == NULL && expected_path_index == NULL) || !strcmp(repo->path_index, expected_path_index));

	git_repository_free(repo);
	must_pass(rmdir_recurs(working_directory));
}

BEGIN_TEST("repo_initialization", init_standard_repo)
	char path_index[GIT_PATH_MAX], path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, WORK_TREE_WITH_TRAILING_SLASH, GIT_DIR);
	git__joinpath(path_index, path_repository, GIT_INDEX_FILE);

	ensure_repository_init(_gittest, WORK_TREE_WITH_TRAILING_SLASH, STANDARD_REPOSITORY, path_index, path_repository, WORK_TREE_WITH_TRAILING_SLASH);
	ensure_repository_init(_gittest, WORK_TREE_WITHOUT_TRAILING_SLASH, STANDARD_REPOSITORY, path_index, path_repository, WORK_TREE_WITH_TRAILING_SLASH);
END_TEST

BEGIN_TEST("repo_initialization", init_bare_repo)
	char path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, WORK_TREE_WITH_TRAILING_SLASH, "");

	ensure_repository_init(_gittest, WORK_TREE_WITH_TRAILING_SLASH, BARE_REPOSITORY, NULL, path_repository, NULL);
	ensure_repository_init(_gittest, WORK_TREE_WITHOUT_TRAILING_SLASH, BARE_REPOSITORY, NULL, path_repository, NULL);
END_TEST

git_testsuite *libgit2_suite_repository(void)
{
	git_testsuite *suite = git_testsuite_new("Repository");

	ADD_TEST(suite, "odb", backend_sorting);
	ADD_TEST(suite, "odb", backend_alternates_sorting);
	ADD_TEST(suite, "repo_initialization", init_standard_repo);
	ADD_TEST(suite, "repo_initialization", init_bare_repo);

	return suite;
}
