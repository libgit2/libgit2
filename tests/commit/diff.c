#include "clar_libgit2.h"
#include "commit.h"
#include "git2/commit.h"

static git_repository *_repo;

void test_commit_diff__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&_repo, "testrepo.git"));
}

void test_commit_diff__cleanup(void)
{
	git_repository_free(_repo);
	_repo = NULL;

	cl_fixture_cleanup("testrepo.git");
}

void test_commit_diff__single_parent(void)
{
	git_commit *commit;
	git_diff *diff;
	git_patch *patch;
	git_oid oid;
	git_buf buf = GIT_BUF_INIT;

	git_oid_fromstr(&oid, "9fd738e8f7967c078dceed8190330fc8648ee56a");
	cl_git_pass(git_commit_lookup(&commit, _repo, &oid));
	cl_git_pass(git_commit_diff(&diff, commit, 0, NULL));
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&buf, patch));

	cl_assert(strcmp(buf.ptr,
		"diff --git a/new.txt b/new.txt\n" \
		"index fa49b07..a71586c 100644\n" \
		"--- a/new.txt\n" \
		"+++ b/new.txt\n" \
		"@@ -1 +1 @@\n" \
		"-new file\n" \
		"+my new file\n") == 0);

	git_buf_free(&buf);
	git_patch_free(patch);
	git_diff_free(diff);
	git_commit_free(commit);
}

void test_commit_diff__first_parent(void)
{
	git_commit *commit;
	git_diff *diff;
	git_patch *patch;
	git_oid oid;
	git_buf buf = GIT_BUF_INIT;

	git_oid_fromstr(&oid, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	cl_git_pass(git_commit_lookup(&commit, _repo, &oid));
	cl_git_pass(git_commit_diff(&diff, commit, 1, NULL));
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&buf, patch));

	cl_assert(strcmp(buf.ptr,
		"diff --git a/branch_file.txt b/branch_file.txt\n" \
		"new file mode 100644\n" \
		"index 0000000..45b983b\n" \
		"--- /dev/null\n" \
		"+++ b/branch_file.txt\n" \
		"@@ -0,0 +1 @@\n" \
		"+hi\n") == 0);

	git_buf_free(&buf);
	git_patch_free(patch);
	git_diff_free(diff);
	git_commit_free(commit);
}

void test_commit_diff__second_parent(void)
{
	git_commit *commit;
	git_diff *diff;
	git_patch *patch;
	git_oid oid;
	git_buf buf = GIT_BUF_INIT;

	git_oid_fromstr(&oid, "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	cl_git_pass(git_commit_lookup(&commit, _repo, &oid));
	cl_git_pass(git_commit_diff(&diff, commit, 2, NULL));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&buf, patch));

	cl_assert(strcmp(buf.ptr,
		"diff --git a/README b/README\n" \
		"index 1385f26..a823312 100644\n" \
		"--- a/README\n" \
		"+++ b/README\n" \
		"@@ -1 +1 @@\n" \
		"-hey\n" \
		"+hey there\n") == 0);

	git_buf_free(&buf);
	git_patch_free(patch);

	cl_git_pass(git_patch_from_diff(&patch, diff, 1));
	cl_git_pass(git_patch_to_buf(&buf, patch));

	cl_assert(strcmp(buf.ptr,
		"diff --git a/new.txt b/new.txt\n" \
		"index fa49b07..a71586c 100644\n" \
		"--- a/new.txt\n" \
		"+++ b/new.txt\n" \
		"@@ -1 +1 @@\n" \
		"-new file\n" \
		"+my new file\n") == 0);

	git_buf_free(&buf);
	git_patch_free(patch);
	git_diff_free(diff);
	git_commit_free(commit);
}

void test_commit_diff__root(void)
{
	git_commit *commit;
	git_diff *diff;
	git_patch *patch;
	git_oid oid;
	git_buf buf = GIT_BUF_INIT;

	git_oid_fromstr(&oid, "8496071c1b46c854b31185ea97743be6a8774479");
	cl_git_pass(git_commit_lookup(&commit, _repo, &oid));
	cl_git_pass(git_commit_diff(&diff, commit, 0, NULL));
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&buf, patch));

	cl_assert(strcmp(buf.ptr,
		"diff --git a/README b/README\n" \
		"new file mode 100644\n" \
		"index 0000000..1385f26\n" \
		"--- /dev/null\n" \
		"+++ b/README\n" \
		"@@ -0,0 +1 @@\n" \
		"+hey\n") == 0);

	git_buf_free(&buf);
	git_patch_free(patch);
	git_diff_free(diff);
	git_commit_free(commit);
}

