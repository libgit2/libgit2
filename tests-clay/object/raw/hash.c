
#include "clay_libgit2.h"

#include "odb.h"
#include "hash.h"

#include "data.h"

static int hash_object(git_oid *oid, git_rawobj *obj)
{
	return git_odb_hash(oid, obj->data, obj->len, obj->type);
}

static char *hello_id = "22596363b3de40b06f981fb85d82312e8c0ed511";
static char *hello_text = "hello world\n";

static char *bye_id = "ce08fe4884650f067bd5703b6a59a8b3b3c99a09";
static char *bye_text = "bye world\n";

void test_object_raw_hash__hash_by_blocks(void)
{
    git_hash_ctx *ctx;
    git_oid id1, id2;

    cl_assert((ctx = git_hash_new_ctx()) != NULL);

	/* should already be init'd */
    git_hash_update(ctx, hello_text, strlen(hello_text));
    git_hash_final(&id2, ctx);
    cl_git_pass(git_oid_fromstr(&id1, hello_id));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);

	/* reinit should permit reuse */
    git_hash_init(ctx);
    git_hash_update(ctx, bye_text, strlen(bye_text));
    git_hash_final(&id2, ctx);
    cl_git_pass(git_oid_fromstr(&id1, bye_id));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);

    git_hash_free_ctx(ctx);
}

void test_object_raw_hash__hash_buffer_in_single_call(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, hello_id));
    git_hash_buf(&id2, hello_text, strlen(hello_text));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_vector(void)
{
    git_oid id1, id2;
    git_buf_vec vec[2];

    cl_git_pass(git_oid_fromstr(&id1, hello_id));

    vec[0].data = hello_text;
    vec[0].len  = 4;
    vec[1].data = hello_text+4;
    vec[1].len  = strlen(hello_text)-4;

    git_hash_vec(&id2, vec, 2);

    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_junk_data(void)
{
    git_oid id, id_zero;

    cl_git_pass(git_oid_fromstr(&id_zero, zero_id));

    /* invalid types: */
    junk_obj.data = some_data;
    cl_git_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ__EXT1;
    cl_git_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ__EXT2;
    cl_git_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ_OFS_DELTA;
    cl_git_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ_REF_DELTA;
    cl_git_fail(hash_object(&id, &junk_obj));

    /* data can be NULL only if len is zero: */
    junk_obj.type = GIT_OBJ_BLOB;
    junk_obj.data = NULL;
    cl_git_pass(hash_object(&id, &junk_obj));
    cl_assert(git_oid_cmp(&id, &id_zero) == 0);

    junk_obj.len = 1;
    cl_git_fail(hash_object(&id, &junk_obj));
}

void test_object_raw_hash__hash_commit_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, commit_id));
    cl_git_pass(hash_object(&id2, &commit_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_tree_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, tree_id));
    cl_git_pass(hash_object(&id2, &tree_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_tag_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, tag_id));
    cl_git_pass(hash_object(&id2, &tag_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_zero_length_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, zero_id));
    cl_git_pass(hash_object(&id2, &zero_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_one_byte_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, one_id));
    cl_git_pass(hash_object(&id2, &one_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_two_byte_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, two_id));
    cl_git_pass(hash_object(&id2, &two_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}

void test_object_raw_hash__hash_multi_byte_object(void)
{
    git_oid id1, id2;

    cl_git_pass(git_oid_fromstr(&id1, some_id));
    cl_git_pass(hash_object(&id2, &some_obj));
    cl_assert(git_oid_cmp(&id1, &id2) == 0);
}
