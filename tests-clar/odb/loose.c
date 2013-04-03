#include "clar_libgit2.h"
#include "odb.h"
#include "posix.h"
#include "loose_data.h"

static void write_object_files(object_data *d)
{
	int fd;

	if (p_mkdir(d->dir, GIT_OBJECT_DIR_MODE) < 0)
		cl_assert(errno == EEXIST);

	cl_assert((fd = p_creat(d->file, S_IREAD | S_IWRITE)) >= 0);
	cl_must_pass(p_write(fd, d->bytes, d->blen));

	p_close(fd);
}

static void cmp_objects(git_rawobj *o, object_data *d)
{
	cl_assert(o->type == git_object_string2type(d->type));
	cl_assert(o->len == d->dlen);

	if (o->len > 0)
		cl_assert(memcmp(o->data, d->data, o->len) == 0);
}

static void test_read_object(object_data *data)
{
    git_oid id;
    git_odb_object *obj;
	git_odb *odb;
	git_rawobj tmp;

    write_object_files(data);

    cl_git_pass(git_odb_open(&odb, "test-objects"));
    cl_git_pass(git_oid_fromstr(&id, data->id));
    cl_git_pass(git_odb_read(&obj, odb, &id));

	tmp.data = obj->buffer;
	tmp.len = obj->cached.size;
	tmp.type = obj->cached.type;

    cmp_objects(&tmp, data);

    git_odb_object_free(obj);
	git_odb_free(odb);
}

void test_odb_loose__initialize(void)
{
	cl_must_pass(p_mkdir("test-objects", GIT_OBJECT_DIR_MODE));
}

void test_odb_loose__cleanup(void)
{
	cl_fixture_cleanup("test-objects");
}

void test_odb_loose__exists(void)
{
    git_oid id, id2;
	git_odb *odb;

    write_object_files(&one);
	cl_git_pass(git_odb_open(&odb, "test-objects"));

    cl_git_pass(git_oid_fromstr(&id, one.id));

    cl_assert(git_odb_exists(odb, &id));

	/* Test for a non-existant object */
    cl_git_pass(git_oid_fromstr(&id2, "8b137891791fe96927ad78e64b0aad7bded08baa"));
    cl_assert(!git_odb_exists(odb, &id2));

	git_odb_free(odb);
}

void test_odb_loose__simple_reads(void)
{
	test_read_object(&commit);
	test_read_object(&tree);
	test_read_object(&tag);
	test_read_object(&zero);
	test_read_object(&one);
	test_read_object(&two);
	test_read_object(&some);
}
