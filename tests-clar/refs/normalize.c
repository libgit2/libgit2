#include "clar_libgit2.h"

#include "repository.h"
#include "git2/reflog.h"
#include "reflog.h"


// Helpers
static int ensure_refname_normalized(int is_oid_ref, const char *input_refname, const char *expected_refname)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_REFNAME_MAX];

	if (is_oid_ref)
		error = git_reference__normalize_name_oid(buffer_out, sizeof(buffer_out), input_refname);
	else
		error = git_reference__normalize_name(buffer_out, sizeof(buffer_out), input_refname);

	if (error < GIT_SUCCESS)
		return error;

	if (expected_refname == NULL)
		return error;

	if (strcmp(buffer_out, expected_refname))
		error = GIT_ERROR;

	return error;
}

#define OID_REF 1
#define SYM_REF 0



void test_refs_normalize__direct(void)
{
   // normalize a direct (OID) reference name
	cl_git_fail(ensure_refname_normalized(OID_REF, "a", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/a/", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.lock", NULL));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/dummy/a", NULL));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/stash", NULL));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/tags/a", "refs/tags/a"));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/heads/a/b", "refs/heads/a/b"));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/heads/a./b", "refs/heads/a./b"));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo?bar", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads\foo", NULL));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs/heads/v@ation", "refs/heads/v@ation"));
	cl_git_pass(ensure_refname_normalized(OID_REF, "refs///heads///a", "refs/heads/a"));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/.a/b", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo/../bar", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo..bar", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/./foo", NULL));
	cl_git_fail(ensure_refname_normalized(OID_REF, "refs/heads/v@{ation", NULL));
}

void test_refs_normalize__symbolic(void)
{
   // normalize a symbolic reference name
	cl_git_pass(ensure_refname_normalized(SYM_REF, "a", "a"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "a/b", "a/b"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs///heads///a", "refs/heads/a"));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "heads\foo", NULL));
}

/* Ported from JGit, BSD licence.
 * See https://github.com/spearce/JGit/commit/e4bf8f6957bbb29362575d641d1e77a02d906739 */
void test_refs_normalize__jgit_suite(void)
{
   // tests borrowed from JGit

/* EmptyString */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "/", NULL));

/* MustHaveTwoComponents */
	cl_git_fail(ensure_refname_normalized(OID_REF, "master", NULL));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "heads/master", "heads/master"));

/* ValidHead */

	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/master", "refs/heads/master"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/pu", "refs/heads/pu"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/z", "refs/heads/z"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/FoO", "refs/heads/FoO"));

/* ValidTag */
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/tags/v1.0", "refs/tags/v1.0"));

/* NoLockSuffix */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master.lock", NULL));

/* NoDirectorySuffix */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master/", NULL));

/* NoSpace */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/i haz space", NULL));

/* NoAsciiControlCharacters */
	{
		char c;
		char buffer[GIT_REFNAME_MAX];
		for (c = '\1'; c < ' '; c++) {
			strncpy(buffer, "refs/heads/mast", 15);
			strncpy(buffer + 15, (const char *)&c, 1);
			strncpy(buffer + 16, "er", 2);
			buffer[18 - 1] = '\0';
			cl_git_fail(ensure_refname_normalized(SYM_REF, buffer, NULL));
		}
	}

/* NoBareDot */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/.", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/..", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/./master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/../master", NULL));

/* NoLeadingOrTrailingDot */
	cl_git_fail(ensure_refname_normalized(SYM_REF, ".", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/.bar", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/..bar", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/bar.", NULL));

/* ContainsDot */
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/m.a.s.t.e.r", "refs/heads/m.a.s.t.e.r"));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master..pu", NULL));

/* NoMagicRefCharacters */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master^", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/^master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "^refs/heads/master", NULL));

	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master~", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/~master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "~refs/heads/master", NULL));

	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master:", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/:master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, ":refs/heads/master", NULL));

/* ShellGlob */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master?", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/?master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "?refs/heads/master", NULL));

	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master[", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/[master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "[refs/heads/master", NULL));

	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master*", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/*master", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "*refs/heads/master", NULL));

/* ValidSpecialCharacters */
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/!", "refs/heads/!"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/\"", "refs/heads/\""));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/#", "refs/heads/#"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/$", "refs/heads/$"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/%", "refs/heads/%"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/&", "refs/heads/&"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/'", "refs/heads/'"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/(", "refs/heads/("));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/)", "refs/heads/)"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/+", "refs/heads/+"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/,", "refs/heads/,"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/-", "refs/heads/-"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/;", "refs/heads/;"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/<", "refs/heads/<"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/=", "refs/heads/="));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/>", "refs/heads/>"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/@", "refs/heads/@"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/]", "refs/heads/]"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/_", "refs/heads/_"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/`", "refs/heads/`"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/{", "refs/heads/{"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/|", "refs/heads/|"));
	cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/}", "refs/heads/}"));

	// This is valid on UNIX, but not on Windows
	// hence we make in invalid due to non-portability
	//
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/\\", NULL));

/* UnicodeNames */
	/*
	 * Currently this fails.
	 * cl_git_pass(ensure_refname_normalized(SYM_REF, "refs/heads/\u00e5ngstr\u00f6m", "refs/heads/\u00e5ngstr\u00f6m"));
	 */

/* RefLogQueryIsValidRef */
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master@{1}", NULL));
	cl_git_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master@{1.hour.ago}", NULL));
}
