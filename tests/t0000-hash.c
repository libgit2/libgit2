#include "test_lib.h"
#include "hash.h"
#include <git/oid.h>

static char *hello_id = "22596363b3de40b06f981fb85d82312e8c0ed511";
static char *hello_text = "hello world\n";

static char *bye_id = "ce08fe4884650f067bd5703b6a59a8b3b3c99a09";
static char *bye_text = "bye world\n";

BEGIN_TEST(hash_iuf)
    git_hash_ctx *ctx;
    git_oid id1, id2;

    must_be_true((ctx = git_hash_new_ctx()) != NULL);

	/* should already be init'd */
    git_hash_update(ctx, hello_text, strlen(hello_text));
    git_hash_final(&id2, ctx);
    must_pass(git_oid_mkstr(&id1, hello_id));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);

	/* reinit should permit reuse */
    git_hash_init(ctx);
    git_hash_update(ctx, bye_text, strlen(bye_text));
    git_hash_final(&id2, ctx);
    must_pass(git_oid_mkstr(&id1, bye_id));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);

    git_hash_free_ctx(ctx);
END_TEST

BEGIN_TEST(hash_buf)
    git_oid id1, id2;

    must_pass(git_oid_mkstr(&id1, hello_id));

    git_hash_buf(&id2, hello_text, strlen(hello_text));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(hash_vec)
    git_oid id1, id2;
    git_buf_vec vec[2];

    must_pass(git_oid_mkstr(&id1, hello_id));

    vec[0].data = hello_text;
    vec[0].len  = 4;
    vec[1].data = hello_text+4;
    vec[1].len  = strlen(hello_text)-4;

    git_hash_vec(&id2, vec, 2);

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST
