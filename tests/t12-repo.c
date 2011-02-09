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

git_testsuite *libgit2_suite_repository(void)
{
	git_testsuite *suite = git_testsuite_new("Repository");

	ADD_TEST(suite, "odb", backend_sorting);
	ADD_TEST(suite, "odb", backend_alternates_sorting);

	return suite;
}
