#include "clar_libgit2.h"
#include "iterator.h"
#include "vector.h"

static git_repository *_repo;
static git_revwalk *_revwalker;
static git_packbuilder *_packbuilder;
static git_indexer *_indexer;
static git_vector _commits;

void test_pack_packbuilder__initialize(void)
{
	cl_git_pass(git_repository_open(&_repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_revwalk_new(&_revwalker, _repo));
	cl_git_pass(git_packbuilder_new(&_packbuilder, _repo));
	cl_git_pass(git_vector_init(&_commits, 0, NULL));
}

void test_pack_packbuilder__cleanup(void)
{
	git_oid *o;
	unsigned int i;

	git_vector_foreach(&_commits, i, o) {
		git__free(o);
	}
	git_vector_free(&_commits);
	git_packbuilder_free(_packbuilder);
	git_revwalk_free(_revwalker);
	git_indexer_free(_indexer);
	git_repository_free(_repo);
}

void test_pack_packbuilder__create_pack(void)
{
	git_indexer_stats stats;
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
					git_commit_tree_oid((git_commit *)obj)));
		git_object_free(obj);
	}

	cl_git_pass(git_packbuilder_write(_packbuilder, "testpack.pack"));

	cl_git_pass(git_indexer_new(&_indexer, "testpack.pack"));
	cl_git_pass(git_indexer_run(_indexer, &stats));
	cl_git_pass(git_indexer_write(_indexer));
}
