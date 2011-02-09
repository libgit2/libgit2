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
#include "t03-data.h"

#include "fileops.h"
#include "git2/odb_backend.h"

static int cmp_objects(git_rawobj *o1, git_rawobj *o2)
{
	if (o1->type != o2->type)
		return -1;
	if (o1->len != o2->len)
		return -1;
	if ((o1->len > 0) && (memcmp(o1->data, o2->data, o1->len) != 0))
		return -1;
	return 0;
}

static git_odb *open_sqlite_odb(void)
{
#ifdef GIT2_SQLITE_BACKEND
	git_odb *odb;
	git_odb_backend *sqlite;

	if (git_odb_new(&odb) < GIT_SUCCESS)
		return NULL;

	if (git_odb_backend_sqlite(&sqlite, ":memory") < GIT_SUCCESS)
		return NULL;

	if (git_odb_add_backend(odb, sqlite, 0) < GIT_SUCCESS)
		return NULL;

	return odb;
#else
	return NULL;
#endif
}

#define TEST_WRITE(PTR) {\
    git_odb *db; \
	git_oid id1, id2; \
    git_rawobj obj; \
	db = open_sqlite_odb(); \
	must_be_true(db != NULL); \
    must_pass(git_oid_mkstr(&id1, PTR.id)); \
    must_pass(git_odb_write(&id2, db, &PTR##_obj)); \
    must_be_true(git_oid_cmp(&id1, &id2) == 0); \
    must_pass(git_odb_read(&obj, db, &id1)); \
    must_pass(cmp_objects(&obj, &PTR##_obj)); \
    git_rawobj_close(&obj); \
    git_odb_close(db); \
}

BEGIN_TEST("sqlite", sql_write_commit)
	TEST_WRITE(commit);
END_TEST

BEGIN_TEST("sqlite", sql_write_tree)
	TEST_WRITE(tree);
END_TEST

BEGIN_TEST("sqlite", sql_write_tag)
	TEST_WRITE(tag);
END_TEST

BEGIN_TEST("sqlite", sql_write_zero)
	TEST_WRITE(zero);
END_TEST

BEGIN_TEST("sqlite", sql_write_one)
	TEST_WRITE(one);
END_TEST

BEGIN_TEST("sqlite", sql_write_two)
	TEST_WRITE(two);
END_TEST

BEGIN_TEST("sqlite", sql_write_some)
	TEST_WRITE(some);
END_TEST


git_testsuite *libgit2_suite_sqlite(void)
{
	git_testsuite *suite = git_testsuite_new("SQLite Backend");

#ifdef GIT2_SQLITE_BACKEND
	ADD_TEST(suite, "sqlite", sql_write_commit);
	ADD_TEST(suite, "sqlite", sql_write_tree);
	ADD_TEST(suite, "sqlite", sql_write_tag);
	ADD_TEST(suite, "sqlite", sql_write_zero);
	ADD_TEST(suite, "sqlite", sql_write_one);
	ADD_TEST(suite, "sqlite", sql_write_two);
	ADD_TEST(suite, "sqlite", sql_write_some);
#endif

	return suite;
}

