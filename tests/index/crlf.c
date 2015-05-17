#include "clar_libgit2.h"
#include "../filter/crlf.h"

#include "git2/checkout.h"
#include "repository.h"
#include "posix.h"

#define FILE_CONTENTS_LF "one\ntwo\nthree\nfour\n"
#define FILE_CONTENTS_CRLF "one\r\ntwo\r\nthree\r\nfour\r\n"
#define FILE_CONTENTS_MORE_CRLF MORE_CRLF_TEXT_RAW
#define FILE_CONTENTS_MORE_LF MORE_LF_TEXT_RAW
#define FILE_CONTENTS_LF_CR MIXED_LF_CR_RAW
#define FILE_CONTENTS_LF_CR_CRLF MIXED_LF_CR_CRLF_RAW
#define FILE_CONTENTS_BINARY_LF "\01" FILE_CONTENTS_LF
#define FILE_CONTENTS_BINARY_CRLF "\01" FILE_CONTENTS_CRLF
#define FILE_CONTENTS_BINARY_LF_CR "\01" FILE_CONTENTS_LF_CR
#define FILE_CONTENTS_BINARY_LF_CR_CRLF "\01" FILE_CONTENTS_LF_CR_CRLF

#define CHECK_CRLF_TO_LF_ERROR cl_assert(giterr_last() != NULL && \
							   giterr_last()->klass == GITERR_FILTER && \
							   strstr(giterr_last()->message, "CRLF would be replaced by LF in") != NULL); \
							   giterr_clear();
#define CHECK_LF_TO_CRLF_ERROR cl_assert(giterr_last() != NULL && \
							   giterr_last()->klass == GITERR_FILTER && \
							   strstr(giterr_last()->message, "LF would be replaced by CRLF in") != NULL); \
							   giterr_clear();

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

/* the following tests are auto-generated, with generate.sh in multitest-folder of crlf-test-generator.7z */
/* no differences for *nix and Windows versions needed */
void test_index_crlf__autocrlf_false__safecrlf_false(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "7fbf4d847b191141d80f30c8ab03d2ad4cd543a9"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "0ff5a53f19bfd2b5eea1ba550295c47515678987"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "04de00b358f13389948756732158eaaaefa1448c"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true__safecrlf_false(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_false(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false__safecrlf_false__text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true__safecrlf_false__text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_false__text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_true__text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_true(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");


	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true__safecrlf_true(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false__safecrlf_true__texteol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_false__safecrlf_true__texteol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__autocrlf_true__safecrlf_true__crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_true__safecrlf_true__no_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "7fbf4d847b191141d80f30c8ab03d2ad4cd543a9"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "0ff5a53f19bfd2b5eea1ba550295c47515678987"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "04de00b358f13389948756732158eaaaefa1448c"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_true__crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__autocrlf_input__safecrlf_true__no_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "7fbf4d847b191141d80f30c8ab03d2ad4cd543a9"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "0ff5a53f19bfd2b5eea1ba550295c47515678987"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "04de00b358f13389948756732158eaaaefa1448c"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true__safecrlf_true__texteol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_true__safecrlf_true__texteol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__autocrlf_false__safecrlf_false__texteol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false__safecrlf_false__texteol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* text eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false__safecrlf_false__crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false__safecrlf_false__no_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "false");
	cl_git_mkfile("./crlf/.gitattributes", "* -crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "7fbf4d847b191141d80f30c8ab03d2ad4cd543a9"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "0ff5a53f19bfd2b5eea1ba550295c47515678987"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "04de00b358f13389948756732158eaaaefa1448c"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "85340755cfe5e28c2835781978bb1cece91b3d0f"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "aaf083a9cb53dac3669dcfa0e48921580d629ec7"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input__safecrlf_true__eol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_input__safecrlf_true__eol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__autocrlf_true__safecrlf_true__eol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_true__safecrlf_true__eol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "true");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__autocrlf_false__safecrlf_true__eol_crlf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=crlf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR
}

void test_index_crlf__autocrlf_false__safecrlf_true__eol_lf_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "false");
	cl_repo_set_string(g_repo, "core.safecrlf", "true");
	cl_git_mkfile("./crlf/.gitattributes", "* eol=lf\n");

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "f384549cbeb481e437091320de6d1f2e15e11b4a"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, "203555c5676d75cd80d69b50beb1f4b588c59ceb"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR
}
