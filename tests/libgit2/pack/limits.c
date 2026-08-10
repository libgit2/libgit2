#include "clar_libgit2.h"

#include <git2.h>
#include "futils.h"

static size_t original_max_object_size = 0;

void test_pack_limits__initialize(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_GET_PACK_MAX_OBJECT_SIZE, &original_max_object_size));
}

void test_pack_limits__cleanup(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_PACK_MAX_OBJECT_SIZE, original_max_object_size));
}

void test_pack_limits__max_object_size(void)
{
	git_indexer *idx;
	git_indexer_options opts = GIT_INDEXER_OPTIONS_INIT;
	git_indexer_progress stats = { 0 };
	git_str pack = GIT_STR_INIT;

	cl_git_pass(git_futils_readbuffer(&pack, cl_fixture("delta_100mb.pack")));

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_PACK_MAX_OBJECT_SIZE, 42 * 1024 * 1024));

	cl_git_pass(git_indexer_new(&idx, ".", &opts));

	cl_git_pass(git_indexer_append(idx, pack.ptr, pack.size, &stats));
	cl_git_fail(git_indexer_commit(idx, &stats));

	git_indexer_free(idx);
	git_str_dispose(&pack);
}
