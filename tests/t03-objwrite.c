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
#include "fileops.h"
#include "odb.h"

static char *odb_dir = "test-objects";
#include "t03-data.h"

static int make_odb_dir(void)
{
	if (p_mkdir(odb_dir, GIT_OBJECT_DIR_MODE) < 0) {
		int err = errno;
		fprintf(stderr, "can't make directory \"%s\"", odb_dir);
		if (err == EEXIST)
			fprintf(stderr, " (already exists)");
		fprintf(stderr, "\n");
		return -1;
	}
	return 0;
}

static int check_object_files(object_data *d)
{
	if (git_futils_exists(d->dir) < 0)
		return -1;
	if (git_futils_exists(d->file) < 0)
		return -1;
	return 0;
}

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

static int remove_object_files(object_data *d)
{
	if (p_unlink(d->file) < 0) {
		fprintf(stderr, "can't delete object file \"%s\"\n", d->file);
		return -1;
	}
	if ((p_rmdir(d->dir) < 0) && (errno != ENOTEMPTY)) {
		fprintf(stderr, "can't remove directory \"%s\"\n", d->dir);
		return -1;
	}

	if (p_rmdir(odb_dir) < 0) {
		fprintf(stderr, "can't remove directory \"%s\"\n", odb_dir);
		return -1;
	}

	return 0;
}

static int streaming_write(git_oid *oid, git_odb *odb, git_rawobj *raw)
{
	git_odb_stream *stream;
	int error;

	if ((error = git_odb_open_wstream(&stream, odb, raw->len, raw->type)) < GIT_SUCCESS)
		return error;

	stream->write(stream, raw->data, raw->len);

	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	return error;
}

BEGIN_TEST(write0, "write loose commit object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, commit.id));

    must_pass(streaming_write(&id2, db, &commit_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&commit));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &commit_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&commit));
END_TEST

BEGIN_TEST(write1, "write loose tree object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, tree.id));

    must_pass(streaming_write(&id2, db, &tree_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&tree));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &tree_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&tree));
END_TEST

BEGIN_TEST(write2, "write loose tag object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, tag.id));

    must_pass(streaming_write(&id2, db, &tag_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&tag));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &tag_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&tag));
END_TEST

BEGIN_TEST(write3, "write zero-length object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, zero.id));

    must_pass(streaming_write(&id2, db, &zero_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&zero));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &zero_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&zero));
END_TEST

BEGIN_TEST(write4, "write one-byte long object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, one.id));

    must_pass(streaming_write(&id2, db, &one_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&one));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &one_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&one));
END_TEST

BEGIN_TEST(write5, "write two-byte long object")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, two.id));

    must_pass(streaming_write(&id2, db, &two_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&two));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &two_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&two));
END_TEST

BEGIN_TEST(write6, "write an object which is several bytes long")
    git_odb *db;
    git_oid id1, id2;
    git_odb_object *obj;

    must_pass(make_odb_dir());
    must_pass(git_odb_open(&db, odb_dir));
    must_pass(git_oid_fromstr(&id1, some.id));

    must_pass(streaming_write(&id2, db, &some_obj));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);
    must_pass(check_object_files(&some));

    must_pass(git_odb_read(&obj, db, &id1));
    must_pass(cmp_objects(&obj->raw, &some_obj));

    git_odb_object_close(obj);
    git_odb_close(db);
    must_pass(remove_object_files(&some));
END_TEST

BEGIN_SUITE(objwrite)
	ADD_TEST(write0);
	ADD_TEST(write1);
	ADD_TEST(write2);
	ADD_TEST(write3);
	ADD_TEST(write4);
	ADD_TEST(write5);
	ADD_TEST(write6);
END_SUITE
