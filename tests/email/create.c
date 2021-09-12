#include "clar.h"
#include "clar_libgit2.h"

#include "buffer.h"

static git_repository *repo;

void test_email_create__initialize(void)
{
	repo = cl_git_sandbox_init("diff_format_email");
}

void test_email_create__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

static void email_for_commit(
	git_buf *out,
	const char *commit_id,
	git_email_create_options *opts)
{
	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;

	git_oid_fromstr(&oid, commit_id);

	cl_git_pass(git_commit_lookup(&commit, repo, &oid));

	cl_git_pass(git_email_create_from_commit(out, commit, opts));

	git_diff_free(diff);
	git_commit_free(commit);
}

static void assert_email_match(
	const char *expected,
	const char *commit_id,
	git_email_create_options *opts)
{
	git_buf buf = GIT_BUF_INIT;

	email_for_commit(&buf, commit_id, opts);
	cl_assert_equal_s(expected, git_buf_cstr(&buf));

	git_buf_dispose(&buf);
}

static void assert_subject_match(
	const char *expected,
	const char *commit_id,
	git_email_create_options *opts)
{
	git_buf buf = GIT_BUF_INIT;
	const char *loc;

	email_for_commit(&buf, commit_id, opts);

	cl_assert((loc = strstr(buf.ptr, "\nSubject: ")) != NULL);
	git_buf_consume(&buf, (loc + 10));
	git_buf_truncate_at_char(&buf, '\n');

	cl_assert_equal_s(expected, git_buf_cstr(&buf));

	git_buf_dispose(&buf);
}

void test_email_create__commit(void)
{
	const char *email =
	"From 9264b96c6d104d0e07ae33d3007b6a48246c6f92 Mon Sep 17 00:00:00 2001\n" \
	"From: Jacques Germishuys <jacquesg@striata.com>\n" \
	"Date: Wed, 9 Apr 2014 20:57:01 +0200\n" \
	"Subject: [PATCH] Modify some content\n" \
	"\n" \
	"---\n" \
	" file1.txt | 8 +++++---\n" \
	" 1 file changed, 5 insertions(+), 3 deletions(-)\n" \
	"\n" \
	"diff --git a/file1.txt b/file1.txt\n" \
	"index 94aaae8..af8f41d 100644\n" \
	"--- a/file1.txt\n" \
	"+++ b/file1.txt\n" \
	"@@ -1,15 +1,17 @@\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	"+_file1.txt_\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	"+\n" \
	"+\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	" file1.txt\n" \
	"-file1.txt\n" \
	"-file1.txt\n" \
	"-file1.txt\n" \
	"+_file1.txt_\n" \
	"+_file1.txt_\n" \
	" file1.txt\n" \
	"--\n" \
	"libgit2 " LIBGIT2_VERSION "\n" \
	"\n";

	assert_email_match(
		email, "9264b96c6d104d0e07ae33d3007b6a48246c6f92", NULL);
}

void test_email_create__mode_change(void)
{
	const char *expected =
	"From 7ade76dd34bba4733cf9878079f9fd4a456a9189 Mon Sep 17 00:00:00 2001\n" \
	"From: Jacques Germishuys <jacquesg@striata.com>\n" \
	"Date: Thu, 10 Apr 2014 10:05:03 +0200\n" \
	"Subject: [PATCH] Update permissions\n" \
	"\n" \
	"---\n" \
	" file1.txt.renamed | 0\n" \
	" 1 file changed, 0 insertions(+), 0 deletions(-)\n" \
	" mode change 100644 => 100755 file1.txt.renamed\n" \
	"\n" \
	"diff --git a/file1.txt.renamed b/file1.txt.renamed\n" \
	"old mode 100644\n" \
	"new mode 100755\n" \
	"--\n" \
	"libgit2 " LIBGIT2_VERSION "\n" \
	"\n";

	assert_email_match(expected, "7ade76dd34bba4733cf9878079f9fd4a456a9189", NULL);
}

void test_email_create__commit_subjects(void)
{
	git_email_create_options opts = GIT_EMAIL_CREATE_OPTIONS_INIT;

	assert_subject_match("[PATCH] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.reroll_number = 42;
	assert_subject_match("[PATCH v42] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.flags |= GIT_EMAIL_CREATE_ALWAYS_NUMBER;
	assert_subject_match("[PATCH v42 1/1] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.start_number = 9;
	assert_subject_match("[PATCH v42 9/9] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.subject_prefix = "";
	assert_subject_match("[v42 9/9] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.reroll_number = 0;
	assert_subject_match("[9/9] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.start_number = 0;
	assert_subject_match("[1/1] Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);

	opts.flags = GIT_EMAIL_CREATE_OMIT_NUMBERS;
	assert_subject_match("Modify some content", "9264b96c6d104d0e07ae33d3007b6a48246c6f92", &opts);
}
