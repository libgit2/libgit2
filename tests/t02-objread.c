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

#include "t02-data.h"
#include "t02-oids.h"

BEGIN_TEST("readloose", read_loose_commit)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &commit));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, commit.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &commit));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &commit));
END_TEST

BEGIN_TEST("readloose", read_loose_tree)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &tree));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, tree.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &tree));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &tree));
END_TEST

BEGIN_TEST("readloose", read_loose_tag)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &tag));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, tag.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &tag));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &tag));
END_TEST

BEGIN_TEST("readloose", read_loose_zero)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &zero));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, zero.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &zero));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &zero));
END_TEST

BEGIN_TEST("readloose", read_loose_one)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &one));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, one.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &one));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &one));
END_TEST

BEGIN_TEST("readloose", read_loose_two)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &two));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, two.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &two));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &two));
END_TEST

BEGIN_TEST("readloose", read_loose_some)
    git_odb *db;
    git_oid id;
    git_rawobj obj;

    must_pass(write_object_files(odb_dir, &some));
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_mkstr(&id, some.id));

    must_pass(git_odb_read(&obj, db, &id));
    must_pass(cmp_objects(&obj, &some));

    git_rawobj_close(&obj);
    git_odb_close(db);
    must_pass(remove_object_files(odb_dir, &some));
END_TEST

BEGIN_TEST("readpack", readpacked_test)
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(packed_objects); ++i) {
		git_oid id;
		git_rawobj obj;

		must_pass(git_oid_mkstr(&id, packed_objects[i]));
		must_be_true(git_odb_exists(db, &id) == 1);
		must_pass(git_odb_read(&obj, db, &id));

		git_rawobj_close(&obj);
	}

    git_odb_close(db);
END_TEST

BEGIN_TEST("readheader", readheader_packed_test)
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(packed_objects); ++i) {
		git_oid id;
		git_rawobj obj, header;

		must_pass(git_oid_mkstr(&id, packed_objects[i]));

		must_pass(git_odb_read(&obj, db, &id));
		must_pass(git_odb_read_header(&header, db, &id));

		must_be_true(obj.len == header.len);
		must_be_true(obj.type == header.type);

		git_rawobj_close(&obj);
	}

    git_odb_close(db); 
END_TEST

BEGIN_TEST("readheader", readheader_loose_test)
	unsigned int i;
    git_odb *db;

    must_pass(git_odb_open(&db, ODB_FOLDER));

	for (i = 0; i < ARRAY_SIZE(loose_objects); ++i) {
		git_oid id;
		git_rawobj obj, header;

		must_pass(git_oid_mkstr(&id, loose_objects[i]));

		must_be_true(git_odb_exists(db, &id) == 1);

		must_pass(git_odb_read(&obj, db, &id));
		must_pass(git_odb_read_header(&header, db, &id));

		must_be_true(obj.len == header.len);
		must_be_true(obj.type == header.type);

		git_rawobj_close(&obj);
	}

    git_odb_close(db);
END_TEST

git_testsuite *libgit2_suite_objread(void)
{
	git_testsuite *suite = git_testsuite_new("Object Read");

	ADD_TEST(suite, "existsloose", exists_loose_one);

	ADD_TEST(suite, "readloose", read_loose_commit);
	ADD_TEST(suite, "readloose", read_loose_tree);
	ADD_TEST(suite, "readloose", read_loose_tag);
	ADD_TEST(suite, "readloose", read_loose_zero);
	ADD_TEST(suite, "readloose", read_loose_one);
	ADD_TEST(suite, "readloose", read_loose_two);
	ADD_TEST(suite, "readloose", read_loose_some);

	/* TODO: import these (naming conflicts) */
/*
	ADD_TEST(suite, "readloose", read_loose_commit_enc);
	ADD_TEST(suite, "readloose", read_loose_tree_enc);
	ADD_TEST(suite, "readloose", read_loose_tag_enc);
	ADD_TEST(suite, "readloose", read_loose_zero_enc);
	ADD_TEST(suite, "readloose", read_loose_one_enc);
	ADD_TEST(suite, "readloose", read_loose_two_enc);
	ADD_TEST(suite, "readloose", read_loose_some_enc);
*/

	ADD_TEST(suite, "readpack", readpacked_test);

	ADD_TEST(suite, "readheader", readheader_packed_test);
	ADD_TEST(suite, "readheader", readheader_loose_test);


	return suite;
}
