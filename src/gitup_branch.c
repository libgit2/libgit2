#include "branch.h"

#include "commit.h"
#include "config.h"
#include "refs.h"
#include "remote.h"

#include "git2/branch.h"
#include "git2/gitup_branch.h"

int gitup_branch_upstream_name(git_buf *out, git_repository *repo, const char *refname)
{
	return git_branch_upstream_name(out, repo, refname);
}

int gitup_branch_upstream_remote(git_buf *buf, git_repository *repo, const char *refname)
{
    return git_branch_upstream_remote(buf, repo, refname);
}

int gitup_branch_upstream_merge(git_buf *buf, git_repository *repo, const char *refname)
{
	return git_branch_upstream_merge(buf, repo, refname);
}
