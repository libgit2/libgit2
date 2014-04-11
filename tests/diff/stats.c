#include "clar.h"
#include "clar_libgit2.h"

#include "buffer.h"
#include "commit.h"
#include "diff.h"

static git_repository *repo;

void test_diff_stats__initialize(void)
{
	repo = cl_git_sandbox_init("diff_format_email");
}

void test_diff_stats__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_diff_stats__stat(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file1.txt | 8 +++++---\n" \
	" 1 file changed, 5 insertions(+), 3 deletions(-)\n";

	git_oid_fromstr(&oid, "9264b96c6d104d0e07ae33d3007b6a48246c6f92");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 1);
	cl_assert(git_diff_stats_insertions(stats) == 5);
	cl_assert(git_diff_stats_deletions(stats) == 3);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__multiple_hunks(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt | 5 +++--\n" \
	" file3.txt | 6 ++++--\n" \
	" 2 files changed, 7 insertions(+), 4 deletions(-)\n";

	git_oid_fromstr(&oid, "cd471f0d8770371e1bc78bcbb38db4c7e4106bd2");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 2);
	cl_assert(git_diff_stats_insertions(stats) == 7);
	cl_assert(git_diff_stats_deletions(stats) == 4);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__numstat(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	"3       2       file2.txt\n"
	"4       2       file3.txt\n";

	git_oid_fromstr(&oid, "cd471f0d8770371e1bc78bcbb38db4c7e4106bd2");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_NUMBER));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__shortstat(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" 1 file changed, 5 insertions(+), 3 deletions(-)\n";

	git_oid_fromstr(&oid, "9264b96c6d104d0e07ae33d3007b6a48246c6f92");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 1);
	cl_assert(git_diff_stats_insertions(stats) == 5);
	cl_assert(git_diff_stats_deletions(stats) == 3);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_SHORT));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt => file2.txt.renamed | 1 +\n"
	" file3.txt => file3.txt.renamed | 4 +++-\n"
	" 2 files changed, 4 insertions(+), 1 deletions(-)\n";

	git_oid_fromstr(&oid, "8947a46e2097638ca6040ad4877246f4186ec3bd");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));
	cl_git_pass(git_diff_find_similar(diff, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 2);
	cl_assert(git_diff_stats_insertions(stats) == 4);
	cl_assert(git_diff_stats_deletions(stats) == 1);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename_nochanges(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt.renamed => file2.txt.renamed2 | 0\n"
	" file3.txt.renamed => file3.txt.renamed2 | 0\n"
	" 2 files changed, 0 insertions(+), 0 deletions(-)\n";

	git_oid_fromstr(&oid, "3991dce9e71a0641ca49a6a4eea6c9e7ff402ed4");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));
	cl_git_pass(git_diff_find_similar(diff, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 2);
	cl_assert(git_diff_stats_insertions(stats) == 0);
	cl_assert(git_diff_stats_deletions(stats) == 0);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename_and_modifiy(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt.renamed2                      | 2 +-\n"
	" file3.txt.renamed2 => file3.txt.renamed | 0\n"
	" 2 files changed, 1 insertions(+), 1 deletions(-)\n";

	git_oid_fromstr(&oid, "4ca10087e696d2ba78d07b146a118e9a7096ed4f");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));
	cl_git_pass(git_diff_find_similar(diff, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 2);
	cl_assert(git_diff_stats_insertions(stats) == 1);
	cl_assert(git_diff_stats_deletions(stats) == 1);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename_no_find(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt         | 5 -----\n"
	" file2.txt.renamed | 6 ++++++\n"
	" file3.txt         | 5 -----\n"
	" file3.txt.renamed | 7 +++++++\n"
	" 4 files changed, 13 insertions(+), 10 deletions(-)\n";

	git_oid_fromstr(&oid, "8947a46e2097638ca6040ad4877246f4186ec3bd");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 4);
	cl_assert(git_diff_stats_insertions(stats) == 13);
	cl_assert(git_diff_stats_deletions(stats) == 10);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename_nochanges_no_find(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt.renamed  | 6 ------\n"
	" file2.txt.renamed2 | 6 ++++++\n"
	" file3.txt.renamed  | 7 -------\n"
	" file3.txt.renamed2 | 7 +++++++\n"
	" 4 files changed, 13 insertions(+), 13 deletions(-)\n";

	git_oid_fromstr(&oid, "3991dce9e71a0641ca49a6a4eea6c9e7ff402ed4");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 4);
	cl_assert(git_diff_stats_insertions(stats) == 13);
	cl_assert(git_diff_stats_deletions(stats) == 13);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__rename_and_modifiy_no_find(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file2.txt.renamed2 | 2 +-\n"
	" file3.txt.renamed  | 7 +++++++\n"
	" file3.txt.renamed2 | 7 -------\n"
	" 3 files changed, 8 insertions(+), 8 deletions(-)\n";

	git_oid_fromstr(&oid, "4ca10087e696d2ba78d07b146a118e9a7096ed4f");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 3);
	cl_assert(git_diff_stats_insertions(stats) == 8);
	cl_assert(git_diff_stats_deletions(stats) == 8);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__binary(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	/* TODO: Actually 0 bytes here should be 5!. Seems like we don't load the new content for binary files? */
	const char *stat =
	" binary.bin | Bin 3 -> 0 bytes\n"
	" 1 file changed, 0 insertions(+), 0 deletions(-)\n";

	git_oid_fromstr(&oid, "8d7523f6fcb2404257889abe0d96f093d9f524f9");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));
	cl_assert(git_diff_stats_files_changed(stats) == 1);
	cl_assert(git_diff_stats_insertions(stats) == 0);
	cl_assert(git_diff_stats_deletions(stats) == 0);

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__binary_numstat(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	"-       -       binary.bin\n";

	git_oid_fromstr(&oid, "8d7523f6fcb2404257889abe0d96f093d9f524f9");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_NUMBER));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}

void test_diff_stats__mode_change(void)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_diff_stats *stats = NULL;
	git_buf buf = GIT_BUF_INIT;

	const char *stat =
	" file1.txt.renamed | 0\n" \
	" 1 file changed, 0 insertions(+), 0 deletions(-)\n" \
	" mode change 100644 => 100755 file1.txt.renamed\n" \
	"\n";

	git_oid_fromstr(&oid, "7ade76dd34bba4733cf9878079f9fd4a456a9189");

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));

	cl_git_pass(git_diff_get_stats(&stats, diff));

	cl_git_pass(git_diff_stats_to_buf(&buf, stats, GIT_DIFF_STATS_FULL | GIT_DIFF_STATS_INCLUDE_SUMMARY));
	cl_assert(strcmp(git_buf_cstr(&buf), stat) == 0);

	git_diff_stats_free(stats);
	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_free(&buf);
}
