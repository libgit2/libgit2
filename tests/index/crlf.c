#include "clar_libgit2.h"
#include "../filter/crlf.h"

#include "git2/checkout.h"
#include "repository.h"
#include "posix.h"

#define FILE_CONTENTS_LF "one\ntwo\nthree\nfour\n"

#define FILE_OID_LF "f384549cbeb481e437091320de6d1f2e15e11b4a"

static git_repository *g_repo;
static git_index *g_index;

void test_index_crlf__initialize(void)
{
	g_repo = cl_git_sandbox_init("crlf");
	cl_git_pass(git_repository_index(&g_index, g_repo));
}

void test_index_crlf__cleanup(void)
{
	git_index_free(g_index);
	cl_git_sandbox_cleanup();
}

void test_index_crlf__autocrlf_false_no_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true_ignored(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input_no_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__safecrlf_true_ignored(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("crlf/newfile.txt", ALL_LF_TEXT_RAW);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", ALL_CRLF_TEXT_RAW);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", MORE_CRLF_TEXT_RAW);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", MORE_LF_TEXT_RAW);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
}
