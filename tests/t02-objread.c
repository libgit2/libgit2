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

#include "t02-data.h"
#include "t02-oids.h"


BEGIN_TEST(existsloose0, "check if a loose object exists on the odb")
    git_odb *db;
    git_oid id, id2;

    must_pass(write_object_files(odb_dir, &one));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, one.id));

    must_be_true(git_odb_exists(db, &id));

	/* Test for a non-existant object */
    must_pass(git_oid_fromstr(&id2, "8b137891791fe96927ad78e64b0aad7bded08baa"));
    must_be_true(0 == git_odb_exists(db, &id2));

    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &one));
END_TEST

BEGIN_TEST(readloose0, "read a loose commit")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &commit));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, commit.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects((git_rawobj *)&obj->raw, &commit));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &commit));
END_TEST

BEGIN_TEST(readloose1, "read a loose tree")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &tree));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, tree.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects((git_rawobj *)&obj->raw, &tree));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &tree));
END_TEST

BEGIN_TEST(readloose2, "read a loose tag")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &tag));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, tag.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects((git_rawobj *)&obj->raw, &tag));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &tag));
END_TEST

BEGIN_TEST(readloose3, "read a loose zero-bytes object")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &zero));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, zero.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects((git_rawobj *)&obj->raw, &zero));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &zero));
END_TEST

BEGIN_TEST(readloose4, "read a one-byte long loose object")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &one));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, one.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj->raw, &one));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &one));
END_TEST

BEGIN_TEST(readloose5, "read a two-bytes long loose object")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &two));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, two.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj->raw, &two));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &two));
END_TEST

BEGIN_TEST(readloose6, "read a loose object which is several bytes long")
    git_odb *db;
    git_oid id;
    git_odb_object *obj;

    must_pass(write_object_files(odb_dir, &some));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id, some.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj->raw, &some));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &some));
END_TEST

BEGIN_TEST(readpack0, "read several packed objects")
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(packed_objects); ++i) {
		git_oid id;
		git_odb_object *obj;

		must_pass(git_oid_fromstr(&id, packed_objects[i]));
		must_be_true(git_odb_exists(db, &id) == 1);
		must_pass(git_odb_read(&obj, db, &id));

		git_odb_object_close(obj);
	}

    git_odb_close(db);
END_TEST

BEGIN_TEST(readheader0, "read only the header of several packed objects")
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(packed_objects); ++i) {
		git_oid id;
		git_odb_object *obj;
		size_t len;
		git_otype type;

		must_pass(git_oid_fromstr(&id, packed_objects[i]));

		must_pass(git_odb_read(&obj, db, &id));
		must_pass(git_odb_read_header(&len, &type, db, &id));

		must_be_true(obj->raw.len == len);
		must_be_true(obj->raw.type == type);

		git_odb_object_close(obj);
	}

    git_odb_close(db);
END_TEST

BEGIN_TEST(readheader1, "read only the header of several loose objects")
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(loose_objects); ++i) {
		git_oid id;
		git_odb_object *obj;
		size_t len;
		git_otype type;

		must_pass(git_oid_fromstr(&id, loose_objects[i]));

		must_be_true(git_odb_exists(db, &id) == 1);

		must_pass(git_odb_read(&obj, db, &id));
		must_pass(git_odb_read_header(&len, &type, db, &id));

		must_be_true(obj->raw.len == len);
		must_be_true(obj->raw.type == type);

		git_odb_object_close(obj);
	}

    git_odb_close(db);
END_TEST

BEGIN_SUITE(objread)
	ADD_TEST(existsloose0);

	ADD_TEST(readloose0);
	ADD_TEST(readloose1);
	ADD_TEST(readloose2);
	ADD_TEST(readloose3);
	ADD_TEST(readloose4);
	ADD_TEST(readloose5);
	ADD_TEST(readloose6);

/*
	ADD_TEST(readloose_enc0);
	ADD_TEST(readloose_enc1);
	ADD_TEST(readloose_enc2);
	ADD_TEST(readloose_enc3);
	ADD_TEST(readloose_enc4);
	ADD_TEST(readloose_enc5);
	ADD_TEST(readloose_enc6);
*/

	ADD_TEST(readpack0);

	ADD_TEST(readheader0);
	ADD_TEST(readheader1);
END_SUITE
