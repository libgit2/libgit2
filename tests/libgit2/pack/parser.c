#include "clar_libgit2.h"
#include <git2.h>
#include "packfile_parser.h"

static git_repository *repo;
static git_str packfile_path = GIT_STR_INIT;

/*
 * The packfile in testrepo, amusingly, does not have the name produced
 * by (modern) `git index-pack`.
 */
#define PACKFILE_NAME "a81e489679b7d3418f9ab594bda8ceb37dd4c695"
#define EXPECTED_HASH "cdd21f629208e17df859e487d2117c0a3939fa10"

void test_pack_parser__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo");
	git_str_joinpath(&packfile_path, clar_sandbox_path(), "testrepo/.git/objects/pack/pack-" PACKFILE_NAME ".pack");
}

void test_pack_parser__cleanup(void)
{
	git_str_dispose(&packfile_path);
	cl_git_sandbox_cleanup();
}

static void index_file(size_t bufsize)
{
	git_indexer *idx;
	char *buf;
	ssize_t ret;
	int fd;

	cl_assert((buf = git__malloc(bufsize)));

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_git_pass(git_indexer_new(&idx, ".", GIT_OID_SHA1, NULL));
#else
	cl_git_pass(git_indexer_new(&idx, ".", 0, NULL, NULL));
#endif

	cl_assert((fd = p_open(packfile_path.ptr, O_RDONLY)) >= 0);

	while ((ret = read(fd, buf, bufsize)) > 0) {
		cl_git_pass(git_indexer_append(idx, buf, (size_t)ret, NULL));
	}

	p_close(fd);

	cl_git_pass(git_indexer_commit(idx, NULL));
	cl_assert_equal_s(EXPECTED_HASH, git_indexer_name(idx));

	git_indexer_free(idx);
	git__free(buf);
}

void test_pack_parser__indexer_single_byte(void)
{
	index_file(1);
}

void test_pack_parser__indexer_reasonable_bufsize(void)
{
	index_file(1024);
}

void test_pack_parser__indexer_entire_file(void)
{
	uint64_t filesize;
	int fd;

	cl_assert((fd = p_open(packfile_path.ptr, O_RDONLY)) >= 0);
	cl_git_pass(git_futils_filesize(&filesize, fd));
	p_close(fd);

	index_file(filesize * 2);
}
