#include "clar_libgit2.h"
#include <git2.h>
#include "packfile_parser.h"

void test_pack_parser__indexer_single_byte(void)
{
	git_indexer *idx;
	char buf[1024];
	ssize_t ret;
	int fd;

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_git_pass(git_indexer_new(&idx, ".", GIT_OID_SHA1, NULL));
#else
	cl_git_pass(git_indexer_new(&idx, ".", 0, NULL, NULL));
#endif

	//cl_assert((fd = p_open("/Users/ethomson/Personal/Projects/libgit2/libgit2-6/tests/resources/testrepo.git/objects/pack/pack-a81e489679b7d3418f9ab594bda8ceb37dd4c695.pack", O_RDONLY)) >= 0);
	cl_assert((fd = p_open("/tmp/pack-b82c9be473f721eacaac5042d11b837f00e7f31e.pack", O_RDONLY)) >= 0);

	while ((ret = read(fd, buf, sizeof(buf))) > 0) {
		cl_git_pass(git_indexer_append(idx, buf, (size_t)ret, NULL));
	}

	p_close(fd);

	cl_git_pass(git_indexer_commit(idx, NULL));

	git_indexer_free(idx);
}
