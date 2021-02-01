#include "clar_libgit2.h"

#include <git2.h>

#include "commit_graph.h"

void test_graph_commit_graph__parse(void)
{
	git_repository *repo;
	struct git_commit_graph_file *cgraph;
	git_buf commit_graph_path = GIT_BUF_INIT;

	cl_git_pass(git_repository_open(&repo, cl_fixture("testrepo.git")));
	cl_git_pass(git_buf_joinpath(&commit_graph_path, git_repository_path(repo), "objects/info/commit-graph"));
	cl_git_pass(git_commit_graph_open(&cgraph, git_buf_cstr(&commit_graph_path)));

	git_commit_graph_free(cgraph);
	git_repository_free(repo);
	git_buf_dispose(&commit_graph_path);
}
