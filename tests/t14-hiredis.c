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
#include "odb.h"

#ifdef GIT2_HIREDIS_BACKEND
#include "t03-data.h"
#include "fileops.h"
#include "git2/odb_backend.h"


static int cmp_objects(git_odb_object *odb_obj, git_rawobj *raw)
{
	if (raw->type != git_odb_object_type(odb_obj))
		return -1;

	if (raw->len != git_odb_object_size(odb_obj))
		return -1;

	if ((raw->len > 0) && (memcmp(raw->data, git_odb_object_data(odb_obj), raw->len) != 0))
		return -1;

	return 0;
}

static git_odb *open_hiredis_odb(void)
{
	git_odb *odb;
	git_odb_backend *hiredis;

	if (git_odb_new(&odb) < GIT_SUCCESS)
		return NULL;

	if (git_odb_backend_hiredis(&hiredis, "127.0.0.1", 6379) < GIT_SUCCESS)
		return NULL;

	if (git_odb_add_backend(odb, hiredis, 0) < GIT_SUCCESS)
		return NULL;

	return odb;
}

#define TEST_WRITE(PTR) {\
    git_odb *db; \
	git_oid id1, id2; \
    git_odb_object *obj; \
	db = open_hiredis_odb(); \
	must_be_true(db != NULL); \
    must_pass(git_oid_mkstr(&id1, PTR.id)); \
    must_pass(git_odb_write(&id2, db, PTR##_obj.data, PTR##_obj.len, PTR##_obj.type)); \
    must_be_true(git_oid_cmp(&id1, &id2) == 0); \
    must_pass(git_odb_read(&obj, db, &id1)); \
    must_pass(cmp_objects(obj, &PTR##_obj)); \
    git_odb_object_close(obj); \
    git_odb_close(db); \
}

BEGIN_TEST(hiredis0, "write a commit, read it back (hiredis backend)")
	TEST_WRITE(commit);
END_TEST

BEGIN_TEST(hiredis1, "write a tree, read it back (hiredis backend)")
	TEST_WRITE(tree);
END_TEST

BEGIN_TEST(hiredis2, "write a tag, read it back (hiredis backend)")
	TEST_WRITE(tag);
END_TEST

BEGIN_TEST(hiredis3, "write a zero-byte entry, read it back (hiredis backend)")
	TEST_WRITE(zero);
END_TEST

BEGIN_TEST(hiredis4, "write a one-byte entry, read it back (hiredis backend)")
	TEST_WRITE(one);
END_TEST

BEGIN_TEST(hiredis5, "write a two-byte entry, read it back (hiredis backend)")
	TEST_WRITE(two);
END_TEST

BEGIN_TEST(hiredis6, "write some bytes in an entry, read it back (hiredis backend)")
	TEST_WRITE(some);
END_TEST


BEGIN_SUITE(hiredis)
	ADD_TEST(hiredis0);
	ADD_TEST(hiredis1);
	ADD_TEST(hiredis2);
	ADD_TEST(hiredis3);
	ADD_TEST(hiredis4);
	ADD_TEST(hiredis5);
	ADD_TEST(hiredis6);
END_SUITE

#else /* no hiredis builtin */
BEGIN_SUITE(hiredis)
	/* empty */
END_SUITE
#endif
