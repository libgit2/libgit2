#include "clar_libgit2.h"
#include "fileops.h"
#include "hash.h"
#include "iterator.h"
#include "vector.h"
#include "posix.h"

static git_repository *_repo;
static git_revwalk *_revwalker;
static git_packbuilder *_packbuilder;
static git_indexer *_indexer;
static git_vector _commits;
static int _commits_is_initialized;

void test_pack_packbuilder__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_revwalk_new(&_revwalker, _repo));
	cl_git_pass(git_packbuilder_new(&_packbuilder, _repo));
	cl_git_pass(git_vector_init(&_commits, 0, NULL));
	_commits_is_initialized = 1;
}

void test_pack_packbuilder__cleanup(void)
{
	git_oid *o;
	unsigned int i;

	if (_commits_is_initialized) {
		_commits_is_initialized = 0;
		git_vector_foreach(&_commits, i, o) {
			git__free(o);
		}
		git_vector_free(&_commits);
	}

	git_packbuilder_free(_packbuilder);
	_packbuilder = NULL;

	git_revwalk_free(_revwalker);
	_revwalker = NULL;

	git_indexer_free(_indexer);
	_indexer = NULL;

	cl_git_sandbox_cleanup();
	_repo = NULL;
}

static void seed_packbuilder(void)
{
	git_oid oid, *o;
	unsigned int i;

	git_revwalk_sorting(_revwalker, GIT_SORT_TIME);
	cl_git_pass(git_revwalk_push_ref(_revwalker, "HEAD"));

	while (git_revwalk_next(&oid, _revwalker) == 0) {
		o = git__malloc(GIT_OID_RAWSZ);
		cl_assert(o != NULL);
		git_oid_cpy(o, &oid);
		cl_git_pass(git_vector_insert(&_commits, o));
	}

	git_vector_foreach(&_commits, i, o) {
		cl_git_pass(git_packbuilder_insert(_packbuilder, o, NULL));
	}

	git_vector_foreach(&_commits, i, o) {
		git_object *obj;
		cl_git_pass(git_object_lookup(&obj, _repo, o, GIT_OBJ_COMMIT));
		cl_git_pass(git_packbuilder_insert_tree(_packbuilder,
					git_commit_tree_id((git_commit *)obj)));
		git_object_free(obj);
	}
}

void test_pack_packbuilder__create_pack(void)
{
	git_transfer_progress stats;
	git_buf buf = GIT_BUF_INIT;
	git_hash_ctx ctx;
	git_oid hash;
	char hex[41]; hex[40] = '\0';

	seed_packbuilder();
	cl_git_pass(git_packbuilder_write(_packbuilder, "testpack.pack"));

	cl_git_pass(git_indexer_new(&_indexer, "testpack.pack"));
	cl_git_pass(git_indexer_run(_indexer, &stats));
	cl_git_pass(git_indexer_write(_indexer));

	/*
	 * By default, packfiles are created with only one thread.
	 * Therefore we can predict the object ordering and make sure
	 * we create exactly the same pack as git.git does when *not*
	 * reusing existing deltas (as libgit2).
	 *
	 * $ cd tests-clar/resources/testrepo.git
	 * $ git rev-list --objects HEAD | \
	 * 	git pack-objects -q --no-reuse-delta --threads=1 pack
	 * $ sha1sum git-80e61eb315239ef3c53033e37fee43b744d57122.pack
	 * 5d410bdf97cf896f9007681b92868471d636954b
	 *
	 */

	cl_git_pass(git_futils_readbuffer(&buf, "testpack.pack"));

	cl_git_pass(git_hash_ctx_init(&ctx));
	cl_git_pass(git_hash_update(&ctx, buf.ptr, buf.size));
	cl_git_pass(git_hash_final(&hash, &ctx));
	git_hash_ctx_cleanup(&ctx);

	git_buf_free(&buf);

	git_oid_fmt(hex, &hash);

	cl_assert_equal_s(hex, "5d410bdf97cf896f9007681b92868471d636954b");
}

static git_transfer_progress stats;
static int foreach_cb(void *buf, size_t len, void *payload)
{
	git_indexer_stream *idx = (git_indexer_stream *) payload;
	cl_git_pass(git_indexer_stream_add(idx, buf, len, &stats));
	return 0;
}

void test_pack_packbuilder__foreach(void)
{
	git_indexer_stream *idx;

	seed_packbuilder();
	cl_git_pass(git_indexer_stream_new(&idx, ".", NULL, NULL));
	cl_git_pass(git_packbuilder_foreach(_packbuilder, foreach_cb, idx));
	cl_git_pass(git_indexer_stream_finalize(idx, &stats));
	git_indexer_stream_free(idx);
}
