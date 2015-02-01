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

#define FILE_OID_LF "f384549cbeb481e437091320de6d1f2e15e11b4a"
#define FILE_OID_CRLF "7fbf4d847b191141d80f30c8ab03d2ad4cd543a9"
#define FILE_OID_MORE_CRLF "0ff5a53f19bfd2b5eea1ba550295c47515678987"
#define FILE_OID_MORE_LF "04de00b358f13389948756732158eaaaefa1448c"
#define FILE_OID_LF_CR "d11e7ef63ba7db1db3b1b99cdbafc57a8549f8a4"
#define FILE_OID_LF_CR_CRLF "417786fc35b3c71aa546e3f95eb5da3c8dad8c41"
#define FILE_OID_BINARY_LF "af6fcf6da196f615d7cda269b55b5c4ecfb4a5b3"
#define FILE_OID_BINARY_CRLF "85340755cfe5e28c2835781978bb1cece91b3d0f"
#define FILE_OID_BINARY_LF_CR "203555c5676d75cd80d69b50beb1f4b588c59ceb"
#define FILE_OID_BINARY_LF_CR_CRLF "aaf083a9cb53dac3669dcfa0e48921580d629ec7"

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

void test_index_crlf__autocrlf_false_no_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid,
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_OID_CRLF : FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_MORE_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_MORE_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile1.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile1.txt"));
	entry = git_index_get_bypath(g_index, "newfile1.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile2.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile2.txt"));
	entry = git_index_get_bypath(g_index, "newfile2.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true_no_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile2.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_LF : FILE_CONTENTS_CRLF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile2.txt"));
	entry = git_index_get_bypath(g_index, "newfile2.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
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

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile7.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile7.txt"));
	entry = git_index_get_bypath(g_index, "newfile7.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input_no_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
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

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile7.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile7.txt"));
	entry = git_index_get_bypath(g_index, "newfile7.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_false_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", false);

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
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

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_true_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_bool(g_repo, "core.autocrlf", true);

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
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

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__autocrlf_input_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");

	cl_git_mkfile("./crlf/newfile.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_CRLF : FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile2.txt",
		(GIT_EOL_NATIVE == GIT_EOL_CRLF) ? FILE_CONTENTS_LF : FILE_CONTENTS_CRLF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile2.txt"));
	entry = git_index_get_bypath(g_index, "newfile2.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_LF_CR_CRLF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR_CRLF));
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

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__safecrlf_true_autocrlf_input_text_auto_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_mkfile("./crlf/.gitattributes", "* text=auto\n");

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile2.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile2.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("./crlf/newfile3.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.txt"));
	entry = git_index_get_bypath(g_index, "newfile3.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.txt"));
	entry = git_index_get_bypath(g_index, "newfile4.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.txt"));
	entry = git_index_get_bypath(g_index, "newfile5.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.txt"));
	entry = git_index_get_bypath(g_index, "newfile6.txt", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__safecrlf_true_autocrlf_input_text__no_attr(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF);

	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
	entry = git_index_get_bypath(g_index, "newfile.txt", 0);

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("./crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
}

void test_index_crlf__safecrlf_true_no_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.txt"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));

	cl_git_mkfile("crlf/newfile.txt", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.txt"));
}

void test_index_crlf__safecrlf_true_text_eol_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf text eol=crlf\n*.lf text eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR
	
	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__safecrlf_true_autocrlf_true_crlf_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf crlf\n*.lf -crlf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
}

void test_index_crlf__safecrlf_true_autocrlf_input_crlf_attrs(void)
{
	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf crlf\n*.lf -crlf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
}

void test_index_crlf__safecrlf_true_autocrlf_input_eol_attrs(void)
{
	cl_repo_set_string(g_repo, "core.autocrlf", "input");
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf eol=crlf\n*.lf eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__safecrlf_true_autocrlf_true_eol_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf eol=crlf\n*.lf eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__safecrlf_true_autocrlf_false_eol_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf eol=crlf\n*.lf eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__safecrlf_true_autocrlf_true_text_eol_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", true);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf text eol=crlf\n*.lf text eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__safecrlf_true_autocrlf_false_text_eol_attrs(void)
{
	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	cl_repo_set_bool(g_repo, "core.safecrlf", true);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf text eol=crlf\n*.lf text eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.crlf"));
	CHECK_LF_TO_CRLF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_fail(git_index_add_bypath(g_index, "newfile.lf"));
	CHECK_CRLF_TO_LF_ERROR
}

void test_index_crlf__attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR));

	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	cl_repo_set_bool(g_repo, "core.safecrlf", false);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf text eol=crlf\n*.lf text eol=lf\n");

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile2.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile2.crlf"));
	entry = git_index_get_bypath(g_index, "newfile2.crlf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile2.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile2.lf"));
	entry = git_index_get_bypath(g_index, "newfile2.lf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);
	
	cl_git_mkfile("./crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);
}

void test_index_crlf__crlf_attrs(void)
{
	const git_index_entry *entry;
	git_oid oid;

	cl_repo_set_bool(g_repo, "core.autocrlf", false);
	cl_repo_set_bool(g_repo, "core.safecrlf", false);

	cl_git_mkfile("./crlf/.gitattributes", "*.crlf crlf\n*.lf -crlf\n");

	cl_git_mkfile("./crlf/newfile.crlf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "37bb7fa3debea1cbb65576733a457347ea1bb74d"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.crlf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "2cda6f203b2f56d5c416b94b28670ec3eafb1398"));
	cl_assert_equal_oid(&oid, &entry->id);
	
	cl_git_mkfile("./crlf/newfile.lf", FILE_CONTENTS_MORE_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "0ff5a53f19bfd2b5eea1ba550295c47515678987"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile.lf", FILE_CONTENTS_MORE_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, "04de00b358f13389948756732158eaaaefa1448c"));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile.crlf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.crlf"));
	entry = git_index_get_bypath(g_index, "newfile.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile.lf", FILE_CONTENTS_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile.lf"));
	entry = git_index_get_bypath(g_index, "newfile.lf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile2.crlf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile2.crlf"));
	entry = git_index_get_bypath(g_index, "newfile2.crlf", 0);
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("crlf/newfile2.lf", FILE_CONTENTS_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile2.lf"));
	entry = git_index_get_bypath(g_index, "newfile2.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile3.crlf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.crlf"));
	entry = git_index_get_bypath(g_index, "newfile3.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.crlf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.crlf"));
	entry = git_index_get_bypath(g_index, "newfile4.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.crlf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.crlf"));
	entry = git_index_get_bypath(g_index, "newfile5.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.crlf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.crlf"));
	entry = git_index_get_bypath(g_index, "newfile6.crlf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile3.lf", FILE_CONTENTS_BINARY_LF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile3.lf"));
	entry = git_index_get_bypath(g_index, "newfile3.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile4.lf", FILE_CONTENTS_BINARY_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile4.lf"));
	entry = git_index_get_bypath(g_index, "newfile4.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile5.lf", FILE_CONTENTS_BINARY_LF_CR);
	cl_git_pass(git_index_add_bypath(g_index, "newfile5.lf"));
	entry = git_index_get_bypath(g_index, "newfile5.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR));
	cl_assert_equal_oid(&oid, &entry->id);

	cl_git_mkfile("./crlf/newfile6.lf", FILE_CONTENTS_BINARY_LF_CR_CRLF);
	cl_git_pass(git_index_add_bypath(g_index, "newfile6.lf"));
	entry = git_index_get_bypath(g_index, "newfile6.lf", 0);
	cl_git_pass(git_oid_fromstr(&oid, FILE_OID_BINARY_LF_CR_CRLF));
	cl_assert_equal_oid(&oid, &entry->id);
}
