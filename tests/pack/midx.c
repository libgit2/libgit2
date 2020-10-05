#include "clar_libgit2.h"

#include <git2.h>

#include "midx.h"

void test_pack_midx__parse(void)
{
	git_repository *repo;
	struct git_midx_file *idx;
	struct git_midx_entry e;
	git_oid id;
	git_buf midx_path = GIT_BUF_INIT;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_buf_joinpath(&midx_path, git_repository_path(repo), "objects/pack/multi-pack-index"));
	cl_git_pass(git_midx_open(&idx, git_buf_cstr(&midx_path)));

	cl_git_pass(git_oid_fromstr(&id, "5001298e0c09ad9c34e4249bc5801c75e9754fa5"));
	cl_git_pass(git_midx_entry_find(&e, idx, &id, GIT_OID_HEXSZ));
	cl_assert_equal_oid(&e.sha1, &id);
	cl_assert_equal_s(
			(const char *)git_vector_get(&idx->packfile_names, e.pack_index),
			"pack-d7c6adf9f61318f041845b01440d09aa7a91e1b5.idx");

	git_midx_free(idx);
	git_repository_free(repo);
	git_buf_dispose(&midx_path);
}
