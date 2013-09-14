#include "clar_libgit2.h"
#include "checkout_helpers.h"
#include "../filter/crlf.h"

#include "git2/checkout.h"
#include "repository.h"
#include "posix.h"

static git_repository *g_repo;

void test_checkout_crlf__initialize(void)
{
	g_repo = cl_git_sandbox_init("crlf");
}

void test_checkout_crlf__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_checkout_crlf__detect_crlf_autocrlf_false(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
}

void test_checkout_crlf__autocrlf_false_index_size_is_unfiltered_size(void)
{
	git_index *index;
	const git_index_entry *entry;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_LF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "all-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_CRLF_TEXT_RAW));

	git_index_free(index);
}

void test_checkout_crlf__detect_crlf_autocrlf_true(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	else
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
}

void test_checkout_crlf__more_lf_autocrlf_true(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	else
		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
}

void test_checkout_crlf__more_crlf_autocrlf_true(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	else
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
}

void test_checkout_crlf__all_crlf_autocrlf_true(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
}

void test_checkout_crlf__autocrlf_true_index_size_is_filtered_size(void)
{
	git_index *index;
	const git_index_entry *entry;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		cl_assert_equal_sz(strlen(ALL_LF_TEXT_RAW), entry->file_size);
	else
		cl_assert_equal_sz(strlen(ALL_LF_TEXT_AS_CRLF), entry->file_size);

	cl_assert((entry = git_index_get_bypath(index, "all-crlf", 0)) != NULL);
	cl_assert_equal_sz(strlen(ALL_CRLF_TEXT_RAW), entry->file_size);

	git_index_free(index);
}

void test_checkout_crlf__with_ident(void)
{
	git_index *index;
	git_blob *blob;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_git_mkfile("crlf/.gitattributes",
		"*.txt text\n*.bin binary\n"
		"*.crlf text eol=crlf\n"
		"*.lf text eol=lf\n"
		"*.ident text ident\n"
		"*.identcrlf ident text eol=crlf\n"
		"*.identlf ident text eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	/* add files with $Id$ */

	cl_git_mkfile("crlf/lf.ident", ALL_LF_TEXT_RAW "\n$Id: initial content$\n");
	cl_git_mkfile("crlf/crlf.ident", ALL_CRLF_TEXT_RAW "\r\n$Id$\r\n\r\n");

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "lf.ident"));
	cl_git_pass(git_index_add_bypath(index, "crlf.ident"));
	cl_repo_commit_from_index(NULL, g_repo, NULL, 0, "Some ident files\n");

	git_checkout_head(g_repo, &opts);

	/* check that blob has $Id$ */

	cl_git_pass(git_blob_lookup(&blob, g_repo,
		& git_index_get_bypath(index, "lf.ident", 0)->oid));
	cl_assert_equal_s(
		ALL_LF_TEXT_RAW "\n$Id$\n", git_blob_rawcontent(blob));

	git_blob_free(blob);

	/* check that filesystem is initially untouched - matching core Git */

	cl_assert_equal_file(
		ALL_LF_TEXT_RAW "\n$Id: initial content$\n", 0, "crlf/lf.ident");

	/* check that forced checkout rewrites correctly */

	p_unlink("crlf/lf.ident");
	p_unlink("crlf/crlflf.ident");

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		cl_assert_equal_file(
			ALL_LF_TEXT_RAW
			"\n$Id: fcf6d4d9c212dc66563b1171b1cd99953c756467$\n",
			0, "crlf/lf.ident");
	else
		cl_assert_equal_file(
			ALL_LF_TEXT_AS_CRLF
			"\r\n$Id: fcf6d4d9c212dc66563b1171b1cd99953c756467$\r\n",
			0, "crlf/lf.ident");

	git_index_free(index);
}
