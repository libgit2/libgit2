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

void test_checkout_crlf__autocrlf_false(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false_index_size_is_unfiltered_size(void)
{
	git_index *index;
	const git_index_entry *entry;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_LF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "all-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_CRLF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "more-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(MORE_LF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "more-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(MORE_CRLF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "mixed-lf-cr", 0)) != NULL);
	cl_assert(entry->file_size == strlen(MIXED_LF_CR_RAW));

	cl_assert((entry = git_index_get_bypath(index, "mixed-lf-cr-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(MIXED_LF_CR_CRLF_RAW));

	cl_assert((entry = git_index_get_bypath(index, "binary-all-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(BINARY_ALL_LF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "binary-all-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(BINARY_ALL_CRLF_TEXT_RAW));

	cl_assert((entry = git_index_get_bypath(index, "binary-mixed-lf-cr", 0)) != NULL);
	cl_assert(entry->file_size == strlen(BINARY_MIXED_LF_CR_RAW));

	cl_assert((entry = git_index_get_bypath(index, "binary-mixed-lf-cr-crlf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(BINARY_MIXED_LF_CR_CRLF_RAW));

	git_index_free(index);
}

void test_checkout_crlf__autocrlf_true(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__detect_crlf_autocrlf_true_utf8(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_repository_set_head(g_repo, "refs/heads/utf8");
	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/few-utf8-chars-lf.txt", FEW_UTF8_CRLF_RAW);
	check_file_contents("./crlf/many-utf8-chars-lf.txt", MANY_UTF8_CRLF_RAW);

	check_file_contents("./crlf/few-utf8-chars-crlf.txt", FEW_UTF8_CRLF_RAW);
	check_file_contents("./crlf/many-utf8-chars-crlf.txt", MANY_UTF8_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true_index_size_is_filtered_size(void)
{
	git_index *index;
	const git_index_entry *entry;
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);
	cl_assert_equal_sz(strlen(ALL_LF_TEXT_AS_CRLF), entry->file_size);

	cl_assert((entry = git_index_get_bypath(index, "all-crlf", 0)) != NULL);
	cl_assert_equal_sz(strlen(ALL_CRLF_TEXT_RAW), entry->file_size);

	git_index_free(index);
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

void test_checkout_crlf__autocrlf_input(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_CRLF) {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
		check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

		check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
		check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
		check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
	} else {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

		check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
		check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

		check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
		check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
		check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
		check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
	}
}

void test_checkout_crlf__autocrlf_true__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_CRLF) {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);
	} else {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);
	}

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_auto_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__text__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__text__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__text__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_false__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	if (GIT_EOL_NATIVE == GIT_EOL_LF) {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

		check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
		check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

		check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
		check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
		check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
		check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
	} else {
		check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

		check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
		check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

		check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
		check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
		check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
		check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
	}
}

void test_checkout_crlf__autocrlf_true__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input__crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__no_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false__text_auto__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_true__text_auto__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_input__text_auto__eol_lf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=lf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_RAW);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_RAW);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_RAW);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_RAW);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_RAW);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_RAW);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_RAW);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_RAW);
}

void test_checkout_crlf__autocrlf_false_text_auto__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_true_text_auto__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__autocrlf_input_text_auto__eol_crlf_attr(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto eol=crlf\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/all-crlf", ALL_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/more-lf", MORE_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/more-crlf", MORE_CRLF_TEXT_AS_CRLF);

	check_file_contents("./crlf/mixed-lf-cr", MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/mixed-lf-cr-crlf", MIXED_LF_CR_CRLF_AS_CRLF);

	check_file_contents("./crlf/binary-all-lf", BINARY_ALL_LF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-all-crlf", BINARY_ALL_CRLF_TEXT_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr", BINARY_MIXED_LF_CR_AS_CRLF);
	check_file_contents("./crlf/binary-mixed-lf-cr-crlf", BINARY_MIXED_LF_CR_CRLF_AS_CRLF);
}

void test_checkout_crlf__can_write_empty_file(void)
{
	git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_FORCE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_repository_set_head(g_repo, "refs/heads/empty-files");
	git_checkout_head(g_repo, &opts);

	check_file_contents("./crlf/test1.txt", "");

	if (GIT_EOL_NATIVE == GIT_EOL_LF)
		check_file_contents("./crlf/test2.txt", "test2.txt's content\n");
	else
		check_file_contents("./crlf/test2.txt", "test2.txt's content\r\n");

	check_file_contents("./crlf/test3.txt", "");
}
