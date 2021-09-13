#include "clar.h"
#include "clar_libgit2.h"

#include "buffer.h"
#include "diff_generate.h"

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

void test_email_create__custom_summary_and_body(void)
{
	const char *expected = "From 627e7e12d87e07a83fad5b6bfa25e86ead4a5270 Mon Sep 17 00:00:00 2001\n" \
	"From: Patrick Steinhardt <ps@pks.im>\n" \
	"Date: Tue, 24 Nov 2015 13:34:39 +0100\n" \
	"Subject: [PPPPPATCH 2/4] This is a subject\n" \
	"\n" \
	"Modify content of file3.txt by appending a new line. Make this\n" \
	"commit message somewhat longer to test behavior with newlines\n" \
	"embedded in the message body.\n" \
	"\n" \
	"Also test if new paragraphs are included correctly.\n" \
	"---\n" \
	" file3.txt | 1 +\n" \
	" 1 file changed, 1 insertion(+)\n" \
	"\n" \
	"diff --git a/file3.txt b/file3.txt\n" \
	"index 9a2d780..7309653 100644\n" \
	"--- a/file3.txt\n" \
	"+++ b/file3.txt\n" \
	"@@ -3,3 +3,4 @@ file3!\n" \
	" file3\n" \
	" file3\n" \
	" file3\n" \
	"+file3\n" \
	"--\n" \
	"libgit2 " LIBGIT2_VERSION "\n" \
	"\n";

	const char *summary = "This is a subject\nwith\nnewlines";
	const char *body = "Modify content of file3.txt by appending a new line. Make this\n" \
	"commit message somewhat longer to test behavior with newlines\n" \
	"embedded in the message body.\n" \
	"\n" \
	"Also test if new paragraphs are included correctly.";

	git_oid oid;
	git_commit *commit = NULL;
	git_diff *diff = NULL;
	git_buf buf = GIT_BUF_INIT;
	git_email_create_options opts = GIT_EMAIL_CREATE_OPTIONS_INIT;

	opts.subject_prefix = "PPPPPATCH";

	git_oid_fromstr(&oid, "627e7e12d87e07a83fad5b6bfa25e86ead4a5270");
	cl_git_pass(git_commit_lookup(&commit, repo, &oid));
	cl_git_pass(git_diff__commit(&diff, repo, commit, NULL));
	cl_git_pass(git_email_create_from_diff(&buf, diff, 2, 4, &oid, summary, body, git_commit_author(commit), &opts));

	cl_assert_equal_s(expected, git_buf_cstr(&buf));

	git_diff_free(diff);
	git_commit_free(commit);
	git_buf_dispose(&buf);
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
