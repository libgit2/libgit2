#include "clar_libgit2.h"

#include "repository.h"
#include "git2/reflog.h"
#include "reflog.h"


// Helpers
static void ensure_refname_normalized(int is_oid_ref,
                                      const char *input_refname,
                                      const char *expected_refname)
{
	char buffer_out[GIT_REFNAME_MAX];

	if (is_oid_ref)
		cl_git_pass(git_reference__normalize_name_oid(buffer_out, sizeof(buffer_out), input_refname));
	else
		cl_git_pass(git_reference__normalize_name(buffer_out, sizeof(buffer_out), input_refname));

   if (expected_refname)
	   cl_assert(0 == strcmp(buffer_out, expected_refname));
}

static void ensure_refname_invalid(int is_oid_ref, const char *input_refname)
{
	char buffer_out[GIT_REFNAME_MAX];

   if (is_oid_ref)
      cl_git_fail(git_reference__normalize_name_oid(buffer_out, sizeof(buffer_out), input_refname));
   else
      cl_git_fail(git_reference__normalize_name(buffer_out, sizeof(buffer_out), input_refname));
}

#define OID_REF 1
#define SYM_REF 0



void test_refs_normalize__direct(void)
{
   // normalize a direct (OID) reference name
	ensure_refname_invalid(OID_REF, "a");
	ensure_refname_invalid(OID_REF, "");
	ensure_refname_invalid(OID_REF, "refs/heads/a/");
	ensure_refname_invalid(OID_REF, "refs/heads/a.");
	ensure_refname_invalid(OID_REF, "refs/heads/a.lock");
	ensure_refname_normalized(OID_REF, "refs/dummy/a", NULL);
	ensure_refname_normalized(OID_REF, "refs/stash", NULL);
	ensure_refname_normalized(OID_REF, "refs/tags/a", "refs/tags/a");
	ensure_refname_normalized(OID_REF, "refs/heads/a/b", "refs/heads/a/b");
	ensure_refname_normalized(OID_REF, "refs/heads/a./b", "refs/heads/a./b");
	ensure_refname_invalid(OID_REF, "refs/heads/foo?bar");
	ensure_refname_invalid(OID_REF, "refs/heads\foo");
	ensure_refname_normalized(OID_REF, "refs/heads/v@ation", "refs/heads/v@ation");
	ensure_refname_normalized(OID_REF, "refs///heads///a", "refs/heads/a");
	ensure_refname_invalid(OID_REF, "refs/heads/.a/b");
	ensure_refname_invalid(OID_REF, "refs/heads/foo/../bar");
	ensure_refname_invalid(OID_REF, "refs/heads/foo..bar");
	ensure_refname_invalid(OID_REF, "refs/heads/./foo");
	ensure_refname_invalid(OID_REF, "refs/heads/v@{ation");
}

void test_refs_normalize__symbolic(void)
{
   // normalize a symbolic reference name
	ensure_refname_normalized(SYM_REF, "a", "a");
	ensure_refname_normalized(SYM_REF, "a/b", "a/b");
	ensure_refname_normalized(SYM_REF, "refs///heads///a", "refs/heads/a");
	ensure_refname_invalid(SYM_REF, "");
	ensure_refname_invalid(SYM_REF, "heads\foo");
}

/* Ported from JGit, BSD licence.
 * See https://github.com/spearce/JGit/commit/e4bf8f6957bbb29362575d641d1e77a02d906739 */
void test_refs_normalize__jgit_suite(void)
{
   // tests borrowed from JGit

/* EmptyString */
	ensure_refname_invalid(SYM_REF, "");
	ensure_refname_invalid(SYM_REF, "/");

/* MustHaveTwoComponents */
	ensure_refname_invalid(OID_REF, "master");
	ensure_refname_normalized(SYM_REF, "heads/master", "heads/master");

/* ValidHead */

	ensure_refname_normalized(SYM_REF, "refs/heads/master", "refs/heads/master");
	ensure_refname_normalized(SYM_REF, "refs/heads/pu", "refs/heads/pu");
	ensure_refname_normalized(SYM_REF, "refs/heads/z", "refs/heads/z");
	ensure_refname_normalized(SYM_REF, "refs/heads/FoO", "refs/heads/FoO");

/* ValidTag */
	ensure_refname_normalized(SYM_REF, "refs/tags/v1.0", "refs/tags/v1.0");

/* NoLockSuffix */
	ensure_refname_invalid(SYM_REF, "refs/heads/master.lock");

/* NoDirectorySuffix */
	ensure_refname_invalid(SYM_REF, "refs/heads/master/");

/* NoSpace */
	ensure_refname_invalid(SYM_REF, "refs/heads/i haz space");

/* NoAsciiControlCharacters */
	{
		char c;
		char buffer[GIT_REFNAME_MAX];
		for (c = '\1'; c < ' '; c++) {
			strncpy(buffer, "refs/heads/mast", 15);
			strncpy(buffer + 15, (const char *)&c, 1);
			strncpy(buffer + 16, "er", 2);
			buffer[18 - 1] = '\0';
			ensure_refname_invalid(SYM_REF, buffer);
		}
	}

/* NoBareDot */
	ensure_refname_invalid(SYM_REF, "refs/heads/.");
	ensure_refname_invalid(SYM_REF, "refs/heads/..");
	ensure_refname_invalid(SYM_REF, "refs/heads/./master");
	ensure_refname_invalid(SYM_REF, "refs/heads/../master");

/* NoLeadingOrTrailingDot */
	ensure_refname_invalid(SYM_REF, ".");
	ensure_refname_invalid(SYM_REF, "refs/heads/.bar");
	ensure_refname_invalid(SYM_REF, "refs/heads/..bar");
	ensure_refname_invalid(SYM_REF, "refs/heads/bar.");

/* ContainsDot */
	ensure_refname_normalized(SYM_REF, "refs/heads/m.a.s.t.e.r", "refs/heads/m.a.s.t.e.r");
	ensure_refname_invalid(SYM_REF, "refs/heads/master..pu");

/* NoMagicRefCharacters */
	ensure_refname_invalid(SYM_REF, "refs/heads/master^");
	ensure_refname_invalid(SYM_REF, "refs/heads/^master");
	ensure_refname_invalid(SYM_REF, "^refs/heads/master");

	ensure_refname_invalid(SYM_REF, "refs/heads/master~");
	ensure_refname_invalid(SYM_REF, "refs/heads/~master");
	ensure_refname_invalid(SYM_REF, "~refs/heads/master");

	ensure_refname_invalid(SYM_REF, "refs/heads/master:");
	ensure_refname_invalid(SYM_REF, "refs/heads/:master");
	ensure_refname_invalid(SYM_REF, ":refs/heads/master");

/* ShellGlob */
	ensure_refname_invalid(SYM_REF, "refs/heads/master?");
	ensure_refname_invalid(SYM_REF, "refs/heads/?master");
	ensure_refname_invalid(SYM_REF, "?refs/heads/master");

	ensure_refname_invalid(SYM_REF, "refs/heads/master[");
	ensure_refname_invalid(SYM_REF, "refs/heads/[master");
	ensure_refname_invalid(SYM_REF, "[refs/heads/master");

	ensure_refname_invalid(SYM_REF, "refs/heads/master*");
	ensure_refname_invalid(SYM_REF, "refs/heads/*master");
	ensure_refname_invalid(SYM_REF, "*refs/heads/master");

/* ValidSpecialCharacters */
	ensure_refname_normalized(SYM_REF, "refs/heads/!", "refs/heads/!");
	ensure_refname_normalized(SYM_REF, "refs/heads/\"", "refs/heads/\"");
	ensure_refname_normalized(SYM_REF, "refs/heads/#", "refs/heads/#");
	ensure_refname_normalized(SYM_REF, "refs/heads/$", "refs/heads/$");
	ensure_refname_normalized(SYM_REF, "refs/heads/%", "refs/heads/%");
	ensure_refname_normalized(SYM_REF, "refs/heads/&", "refs/heads/&");
	ensure_refname_normalized(SYM_REF, "refs/heads/'", "refs/heads/'");
	ensure_refname_normalized(SYM_REF, "refs/heads/(", "refs/heads/(");
	ensure_refname_normalized(SYM_REF, "refs/heads/)", "refs/heads/)");
	ensure_refname_normalized(SYM_REF, "refs/heads/+", "refs/heads/+");
	ensure_refname_normalized(SYM_REF, "refs/heads/,", "refs/heads/,");
	ensure_refname_normalized(SYM_REF, "refs/heads/-", "refs/heads/-");
	ensure_refname_normalized(SYM_REF, "refs/heads/;", "refs/heads/;");
	ensure_refname_normalized(SYM_REF, "refs/heads/<", "refs/heads/<");
	ensure_refname_normalized(SYM_REF, "refs/heads/=", "refs/heads/=");
	ensure_refname_normalized(SYM_REF, "refs/heads/>", "refs/heads/>");
	ensure_refname_normalized(SYM_REF, "refs/heads/@", "refs/heads/@");
	ensure_refname_normalized(SYM_REF, "refs/heads/]", "refs/heads/]");
	ensure_refname_normalized(SYM_REF, "refs/heads/_", "refs/heads/_");
	ensure_refname_normalized(SYM_REF, "refs/heads/`", "refs/heads/`");
	ensure_refname_normalized(SYM_REF, "refs/heads/{", "refs/heads/{");
	ensure_refname_normalized(SYM_REF, "refs/heads/|", "refs/heads/|");
	ensure_refname_normalized(SYM_REF, "refs/heads/}", "refs/heads/}");

	// This is valid on UNIX, but not on Windows
	// hence we make in invalid due to non-portability
	//
	ensure_refname_invalid(SYM_REF, "refs/heads/\\");

/* UnicodeNames */
	/*
	 * Currently this fails.
	 * ensure_refname_normalized(SYM_REF, "refs/heads/\u00e5ngstr\u00f6m", "refs/heads/\u00e5ngstr\u00f6m");
	 */

/* RefLogQueryIsValidRef */
	ensure_refname_invalid(SYM_REF, "refs/heads/master@{1}");
	ensure_refname_invalid(SYM_REF, "refs/heads/master@{1.hour.ago}");
}
