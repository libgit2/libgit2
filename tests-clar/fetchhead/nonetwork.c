#include "clar_libgit2.h"

#include "repository.h"
#include "fetchhead.h"
#include "fetchhead_data.h"

#define DO_LOCAL_TEST 0

static git_repository *g_repo;

void test_fetchhead_nonetwork__initialize(void)
{
	g_repo = NULL;
}

static void cleanup_repository(void *path)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup((const char *)path);
}

void test_fetchhead_nonetwork__write(void)
{
	git_vector fetchhead_vector;
	git_fetchhead_ref *fetchhead[6];
	git_oid oid[6];
	git_buf fetchhead_buf = GIT_BUF_INIT;
	size_t i;
	int equals = 0;

	git_vector_init(&fetchhead_vector, 6, NULL);

	cl_set_cleanup(&cleanup_repository, "./test1");

	cl_git_pass(git_repository_init(&g_repo, "./test1", 0));

	cl_git_pass(git_oid_fromstr(&oid[0],
		"49322bb17d3acc9146f98c97d078513228bbf3c0"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[0], &oid[0], 1,
		"refs/heads/master",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[0]));

	cl_git_pass(git_oid_fromstr(&oid[1],
		"0966a434eb1a025db6b71485ab63a3bfbea520b6"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[1], &oid[1], 0,
		"refs/heads/first-merge",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[1]));

	cl_git_pass(git_oid_fromstr(&oid[2],
		"42e4e7c5e507e113ebbb7801b16b52cf867b7ce1"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[2], &oid[2], 0,
		"refs/heads/no-parent",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[2]));

	cl_git_pass(git_oid_fromstr(&oid[3],
		"d96c4e80345534eccee5ac7b07fc7603b56124cb"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[3], &oid[3], 0,
		"refs/tags/annotated_tag",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[3]));

	cl_git_pass(git_oid_fromstr(&oid[4],
		"55a1a760df4b86a02094a904dfa511deb5655905"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[4], &oid[4], 0,
		"refs/tags/blob",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[4]));

	cl_git_pass(git_oid_fromstr(&oid[5],
		"8f50ba15d49353813cc6e20298002c0d17b0a9ee"));
	cl_git_pass(git_fetchhead_ref_create(&fetchhead[5], &oid[5], 0,
		"refs/tags/commit_tree",
		"git://github.com/libgit2/TestGitRepository"));
	cl_git_pass(git_vector_insert(&fetchhead_vector, fetchhead[5]));

	git_fetchhead_write(g_repo, &fetchhead_vector);

	cl_git_pass(git_futils_readbuffer(&fetchhead_buf,
		"./test1/.git/FETCH_HEAD"));

	equals = (strcmp(fetchhead_buf.ptr, FETCH_HEAD_WILDCARD_DATA) == 0);

	for (i=0; i < 6; i++)
		git_fetchhead_ref_free(fetchhead[i]);

	git_buf_free(&fetchhead_buf);

	git_vector_free(&fetchhead_vector);

	cl_assert(equals);
}

