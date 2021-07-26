#include "clar_libgit2.h"

#include <git2.h>

#include "commit_graph.h"

void test_graph_commit_graph__parse(void)
{
	git_repository *repo;
	struct git_commit_graph_file *file;
	struct git_commit_graph_entry e, parent;
	git_oid id;
	git_buf commit_graph_path = GIT_BUF_INIT;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_buf_joinpath(&commit_graph_path, git_repository_path(repo), "objects/info/commit-graph"));
	cl_git_pass(git_commit_graph_file_open(&file, git_buf_cstr(&commit_graph_path)));
	cl_assert_equal_i(git_commit_graph_file_needs_refresh(file, git_buf_cstr(&commit_graph_path)), 0);

	cl_git_pass(git_oid_fromstr(&id, "5001298e0c09ad9c34e4249bc5801c75e9754fa5"));
	cl_git_pass(git_commit_graph_entry_find(&e, file, &id, GIT_OID_HEXSZ));
	cl_assert_equal_oid(&e.sha1, &id);
	cl_git_pass(git_oid_fromstr(&id, "418382dff1ffb8bdfba833f4d8bbcde58b1e7f47"));
	cl_assert_equal_oid(&e.tree_oid, &id);
	cl_assert_equal_i(e.generation, 1);
	cl_assert_equal_i(e.commit_time, UINT64_C(1273610423));
	cl_assert_equal_i(e.parent_count, 0);

	cl_git_pass(git_oid_fromstr(&id, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644"));
	cl_git_pass(git_commit_graph_entry_find(&e, file, &id, GIT_OID_HEXSZ));
	cl_assert_equal_oid(&e.sha1, &id);
	cl_assert_equal_i(e.generation, 5);
	cl_assert_equal_i(e.commit_time, UINT64_C(1274813907));
	cl_assert_equal_i(e.parent_count, 2);

	cl_git_pass(git_oid_fromstr(&id, "9fd738e8f7967c078dceed8190330fc8648ee56a"));
	cl_git_pass(git_commit_graph_entry_parent(&parent, file, &e, 0));
	cl_assert_equal_oid(&parent.sha1, &id);
	cl_assert_equal_i(parent.generation, 4);

	cl_git_pass(git_oid_fromstr(&id, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_commit_graph_entry_parent(&parent, file, &e, 1));
	cl_assert_equal_oid(&parent.sha1, &id);
	cl_assert_equal_i(parent.generation, 3);

	git_commit_graph_file_free(file);
	git_repository_free(repo);
	git_buf_dispose(&commit_graph_path);
}

void test_graph_commit_graph__parse_octopus_merge(void)
{
	git_repository *repo;
	struct git_commit_graph_file *file;
	struct git_commit_graph_entry e, parent;
	git_oid id;
	git_buf commit_graph_path = GIT_BUF_INIT;

	cl_git_pass(git_repository_open(&repo, cl_fixture("merge-recursive/.gitted")));
	cl_git_pass(git_buf_joinpath(&commit_graph_path, git_repository_path(repo), "objects/info/commit-graph"));
	cl_git_pass(git_commit_graph_file_open(&file, git_buf_cstr(&commit_graph_path)));

	cl_git_pass(git_oid_fromstr(&id, "d71c24b3b113fd1d1909998c5bfe33b86a65ee03"));
	cl_git_pass(git_commit_graph_entry_find(&e, file, &id, GIT_OID_HEXSZ));
	cl_assert_equal_oid(&e.sha1, &id);
	cl_git_pass(git_oid_fromstr(&id, "348f16ffaeb73f319a75cec5b16a0a47d2d5e27c"));
	cl_assert_equal_oid(&e.tree_oid, &id);
	cl_assert_equal_i(e.generation, 7);
	cl_assert_equal_i(e.commit_time, UINT64_C(1447083009));
	cl_assert_equal_i(e.parent_count, 3);

	cl_git_pass(git_oid_fromstr(&id, "ad2ace9e15f66b3d1138922e6ffdc3ea3f967fa6"));
	cl_git_pass(git_commit_graph_entry_parent(&parent, file, &e, 0));
	cl_assert_equal_oid(&parent.sha1, &id);
	cl_assert_equal_i(parent.generation, 6);

	cl_git_pass(git_oid_fromstr(&id, "483065df53c0f4a02cdc6b2910b05d388fc17ffb"));
	cl_git_pass(git_commit_graph_entry_parent(&parent, file, &e, 1));
	cl_assert_equal_oid(&parent.sha1, &id);
	cl_assert_equal_i(parent.generation, 2);

	cl_git_pass(git_oid_fromstr(&id, "815b5a1c80ca749d705c7aa0cb294a00cbedd340"));
	cl_git_pass(git_commit_graph_entry_parent(&parent, file, &e, 2));
	cl_assert_equal_oid(&parent.sha1, &id);
	cl_assert_equal_i(parent.generation, 6);

	git_commit_graph_file_free(file);
	git_repository_free(repo);
	git_buf_dispose(&commit_graph_path);
}
