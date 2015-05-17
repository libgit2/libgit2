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

void test_checkout_crlf__with_ident(void)
{
	git_index *index;
	git_blob *blob;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

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
	cl_git_mkfile("crlf/more1.identlf", "$Id$\n" MORE_LF_TEXT_RAW);
	cl_git_mkfile("crlf/more2.identcrlf", "\r\n$Id: $\r\n" MORE_CRLF_TEXT_RAW);

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "lf.ident"));
	cl_git_pass(git_index_add_bypath(index, "crlf.ident"));
	cl_git_pass(git_index_add_bypath(index, "more1.identlf"));
	cl_git_pass(git_index_add_bypath(index, "more2.identcrlf"));
	cl_repo_commit_from_index(NULL, g_repo, NULL, 0, "Some ident files\n");

	git_checkout_head(g_repo, &opts);

	/* check that blobs have $Id$ */

	cl_git_pass(git_blob_lookup(&blob, g_repo,
		& git_index_get_bypath(index, "lf.ident", 0)->id));
	cl_assert_equal_s(
		ALL_LF_TEXT_RAW "\n$Id$\n", git_blob_rawcontent(blob));
	git_blob_free(blob);

	cl_git_pass(git_blob_lookup(&blob, g_repo,
		& git_index_get_bypath(index, "more2.identcrlf", 0)->id));
	cl_assert_equal_s(
		"\n$Id$\n" MORE_CRLF_TEXT_AS_LF, git_blob_rawcontent(blob));
	git_blob_free(blob);

	/* check that filesystem is initially untouched - matching core Git */

	cl_assert_equal_file(
		ALL_LF_TEXT_RAW "\n$Id: initial content$\n", 0, "crlf/lf.ident");

	/* check that forced checkout rewrites correctly */

	p_unlink("crlf/lf.ident");
	p_unlink("crlf/crlf.ident");
	p_unlink("crlf/more1.identlf");
	p_unlink("crlf/more2.identcrlf");

	git_checkout_head(g_repo, &opts);

	cl_assert_equal_file(
		ALL_LF_TEXT_AS_CRLF
		"\r\n$Id: fcf6d4d9c212dc66563b1171b1cd99953c756467$\r\n",
		0, "crlf/lf.ident");
	cl_assert_equal_file(
		ALL_CRLF_TEXT_RAW
		"\r\n$Id: f2c66ad9b2b5a734d9bf00d5000cc10a62b8a857$\r\n\r\n",
		0, "crlf/crlf.ident");

	cl_assert_equal_file(
		"$Id: f7830382dac1f1583422be5530fdfbd26289431b$\n"
		MORE_LF_TEXT_AS_LF, 0, "crlf/more1.identlf");

	cl_assert_equal_file(
		"\r\n$Id: 74677a68413012ce8d7e7cfc3f12603df3a3eac4$\r\n"
		MORE_CRLF_TEXT_AS_CRLF, 0, "crlf/more2.identcrlf");

	git_index_free(index);
}

void test_checkout_crlf__can_write_empty_file(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_repository_set_head(g_repo, "refs/heads/empty-files");
	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/test1.txt", "");

	check_file_contents("./crlf/test2.txt", "test2.txt's content\r\n");

	check_file_contents("./crlf/test3.txt", "");
}

// the following tests are auto-generated, with generate.sh in multitest-checkout-folder of crlf-test-generator.7z
#ifdef GIT_WIN32
void test_checkout_crlf__autocrlf_false(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");


	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}
#else
void test_checkout_crlf__autocrlf_false(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");


	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");


	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");


	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__texteol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__texteol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_autoeol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text_autoeol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_AS_CRLF);
}
#endif
