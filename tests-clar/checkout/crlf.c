#include "clar_libgit2.h"
#include "checkout_helpers.h"

#include "git2/checkout.h"
#include "repository.h"

#define UTF8_BOM "\xEF\xBB\xBF"
#define ALL_CRLF_TEXT_RAW	"crlf\r\ncrlf\r\ncrlf\r\ncrlf\r\n"
#define ALL_LF_TEXT_RAW		"lf\nlf\nlf\nlf\nlf\n"
#define MORE_CRLF_TEXT_RAW	"crlf\r\ncrlf\r\nlf\ncrlf\r\ncrlf\r\n"
#define MORE_LF_TEXT_RAW	"lf\nlf\ncrlf\r\nlf\nlf\n"

#define ALL_LF_TEXT_AS_CRLF	"lf\r\nlf\r\nlf\r\nlf\r\nlf\r\n"

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
#ifdef GIT_WIN32
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	test_file_contents("./crlf/all-lf", ALL_LF_TEXT_RAW);
#endif
}

void test_checkout_crlf__autocrlf_false_index_size_is_unfiltered_size(void)
{
#ifdef GIT_WIN32
	git_index *index;
	const git_index_entry *entry;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_LF_TEXT_RAW));

	git_index_free(index);
#endif
}

void test_checkout_crlf__detect_crlf_autocrlf_true(void)
{
#ifdef GIT_WIN32
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	test_file_contents("./crlf/all-lf", ALL_LF_TEXT_AS_CRLF);
#endif
}

void test_checkout_crlf__autocrlf_true_index_size_is_filtered_size(void)
{
#ifdef GIT_WIN32
	git_index *index;
	const git_index_entry *entry;
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;
	opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	git_checkout_head(g_repo, &opts);

	git_repository_index(&index, g_repo);

	cl_assert((entry = git_index_get_bypath(index, "all-lf", 0)) != NULL);
	cl_assert(entry->file_size == strlen(ALL_LF_TEXT_AS_CRLF));

	git_index_free(index);
#endif
}
