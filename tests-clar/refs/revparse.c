#include "clar_libgit2.h"

#include "git2/revparse.h"

static git_repository *g_repo;
static git_object *g_obj;
static char g_orig_tz[16] = {0};



/* Helpers */
static void test_object(const char *spec, const char *expected_oid)
{
	char objstr[64] = {0};

	cl_git_pass(git_revparse_single(&g_obj, g_repo, spec));
	git_oid_fmt(objstr, git_object_id(g_obj));
	cl_assert_equal_s(objstr, expected_oid);

	git_object_free(g_obj);
	g_obj = NULL;
}


void test_refs_revparse__initialize(void)
{
	char *tz = cl_getenv("TZ");
	if (tz)
		strcpy(g_orig_tz, tz);
	cl_setenv("TZ", "UTC");
	g_repo = cl_git_sandbox_init("testrepo.git");
}

void test_refs_revparse__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_obj = NULL;
	cl_setenv("TZ", g_orig_tz);
}

void test_refs_revparse__nonexistant_object(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "this doesn't exist"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "this doesn't exist^1"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "this doesn't exist~2"));
}

void test_refs_revparse__shas(void)
{
	test_object("c47800c7266a2be04c571c04d5a6614691ea99bd", "c47800c7266a2be04c571c04d5a6614691ea99bd");
	test_object("c47800c", "c47800c7266a2be04c571c04d5a6614691ea99bd");
}

void test_refs_revparse__head(void)
{
	test_object("HEAD", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
}

void test_refs_revparse__full_refs(void)
{
	test_object("refs/heads/master", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("refs/heads/test", "e90810b8df3e80c413d903f631643c716887138d");
	test_object("refs/tags/test", "b25fa35b38051e4ae45d4222e795f9df2e43f1d1");
}

void test_refs_revparse__partial_refs(void)
{
	test_object("point_to_blob", "1385f264afb75a56a5bec74243be9b367ba4ca08");
	test_object("packed-test", "4a202b346bb0fb0db7eff3cffeb3c70babbd2045");
	test_object("br2", "a4a7dce85cf63874e984719f4fdd239f5145052f");
}

void test_refs_revparse__describe_output(void)
{
	test_object("blah-7-gc47800c", "c47800c7266a2be04c571c04d5a6614691ea99bd");
	test_object("not-good", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
}

void test_refs_revparse__nth_parent(void)
{
	test_object("be3563a^1", "9fd738e8f7967c078dceed8190330fc8648ee56a");
	test_object("be3563a^", "9fd738e8f7967c078dceed8190330fc8648ee56a");
	test_object("be3563a^2", "c47800c7266a2be04c571c04d5a6614691ea99bd");
	test_object("be3563a^1^1", "4a202b346bb0fb0db7eff3cffeb3c70babbd2045");
	test_object("be3563a^2^1", "5b5b025afb0b4c913b4c338a42934a3863bf3644");
	test_object("be3563a^0", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");

	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "be3563a^42"));
}

void test_refs_revparse__not_tag(void)
{
	test_object("point_to_blob^{}", "1385f264afb75a56a5bec74243be9b367ba4ca08");
	test_object("wrapped_tag^{}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
}

void test_refs_revparse__to_type(void)
{
	test_object("wrapped_tag^{commit}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("wrapped_tag^{tree}", "944c0f6e4dfa41595e6eb3ceecdb14f50fe18162");
	test_object("point_to_blob^{blob}", "1385f264afb75a56a5bec74243be9b367ba4ca08");

	cl_git_fail(git_revparse_single(&g_obj, g_repo, "wrapped_tag^{blob}"));
}

void test_refs_revparse__linear_history(void)
{
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "foo~bar"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "master~bar"));

	test_object("master~0", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("master~1", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("master~2", "9fd738e8f7967c078dceed8190330fc8648ee56a");
	test_object("master~1~1", "9fd738e8f7967c078dceed8190330fc8648ee56a");
}

void test_refs_revparse__chaining(void)
{
	test_object("master~1^1", "9fd738e8f7967c078dceed8190330fc8648ee56a");
	test_object("master~1^2", "c47800c7266a2be04c571c04d5a6614691ea99bd");
	test_object("master^1^2~1", "5b5b025afb0b4c913b4c338a42934a3863bf3644");
	test_object("master^1^1^1^1^1", "8496071c1b46c854b31185ea97743be6a8774479");
}

void test_refs_revparse__reflog(void)
{
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "@{-xyz}"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "@{-0}"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "@{1000}"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "nope@{0}"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "master@{31415}"));

	test_object("@{-2}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("@{-1}", "a4a7dce85cf63874e984719f4fdd239f5145052f");
	test_object("master@{0}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("master@{1}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("@{0}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("@{1}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("master@{upstream}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("master@{u}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
}

void test_refs_revparse__revwalk(void)
{
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "master^{/not found in any commit}"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "master^{/merge}"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, "master^{/((}"));

	test_object("master^{/anoth}", "5b5b025afb0b4c913b4c338a42934a3863bf3644");
	test_object("master^{/Merge}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("br2^{/Merge}", "a4a7dce85cf63874e984719f4fdd239f5145052f");
	test_object("master^{/fo.rth}", "9fd738e8f7967c078dceed8190330fc8648ee56a");
}

void test_refs_revparse__date(void)
{
	/*
	 * $ git reflog HEAD --date=iso
	 * a65fedf HEAD@{2012-04-30 08:23:41 -0900}: checkout: moving from br2 to master
	 * a4a7dce HEAD@{2012-04-30 08:23:37 -0900}: commit: checking in
	 * c47800c HEAD@{2012-04-30 08:23:28 -0900}: checkout: moving from master to br2
	 * a65fedf HEAD@{2012-04-30 08:23:23 -0900}: commit:
	 * be3563a HEAD@{2012-04-30 10:22:43 -0700}: clone: from /Users/ben/src/libgit2/tes
	 *
	 * $ git reflog HEAD --date=raw
	 * a65fedf HEAD@{1335806621 -0900}: checkout: moving from br2 to master
	 * a4a7dce HEAD@{1335806617 -0900}: commit: checking in
	 * c47800c HEAD@{1335806608 -0900}: checkout: moving from master to br2
	 * a65fedf HEAD@{1335806603 -0900}: commit:
	 * be3563a HEAD@{1335806563 -0700}: clone: from /Users/ben/src/libgit2/tests/resour
	 */
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "HEAD@{10 years ago}"));

	test_object("HEAD@{1 second}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("HEAD@{1 second ago}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("HEAD@{2 days ago}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");

	/*
	 * $ git reflog master --date=iso
	 * a65fedf master@{2012-04-30 09:23:23 -0800}: commit: checking in
	 * be3563a master@{2012-04-30 09:22:43 -0800}: clone: from /Users/ben/src...
	 *
	 * $ git reflog master --date=raw
	 * a65fedf master@{1335806603 -0800}: commit: checking in
	 * be3563a master@{1335806563 -0800}: clone: from /Users/ben/src/libgit2/tests/reso
	 */


	/*
	 * $ git reflog -1 "master@{2012-04-30 17:22:42 +0000}"
	 * warning: Log for 'master' only goes back to Mon, 30 Apr 2012 09:22:43 -0800.
	 */
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "master@{2012-04-30 17:22:42 +0000}"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "master@{2012-04-30 09:22:42 -0800}"));

	/*
	 * $ git reflog -1 "master@{2012-04-30 17:22:43 +0000}"
	 * be3563a master@{Mon Apr 30 09:22:43 2012 -0800}: clone: from /Users/ben/src/libg
	 */
	test_object("master@{2012-04-30 17:22:43 +0000}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
	test_object("master@{2012-04-30 09:22:43 -0800}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");

	/*
	 * $ git reflog -1 "master@{2012-4-30 09:23:27 -0800}"
	 * a65fedf master@{Mon Apr 30 09:23:23 2012 -0800}: commit: checking in
	 */
	test_object("master@{2012-4-30 09:23:27 -0800}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");

	/*
	 * $ git reflog -1 master@{2012-05-03}
	 * a65fedf master@{Mon Apr 30 09:23:23 2012 -0800}: commit: checking in
	 */
	test_object("master@{2012-05-03}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");

	/*
	 * $ git reflog -1 "master@{1335806603}"
	 * a65fedf
	 *
	 * $ git reflog -1 "master@{1335806602}"
	 * be3563a
	 */
	test_object("master@{1335806603}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
	test_object("master@{1335806602}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
}

void test_refs_revparse__colon(void)
{
	cl_git_fail(git_revparse_single(&g_obj, g_repo, ":/"));
	cl_git_fail(git_revparse_single(&g_obj, g_repo, ":2:README"));

	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, ":/not found in any commit"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "subtrees:ab/42.txt"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "subtrees:ab/4.txt/nope"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "subtrees:nope"));
	cl_assert_equal_i(GIT_ENOTFOUND, git_revparse_single(&g_obj, g_repo, "test/master^1:branch_file.txt"));

	/* Trees */
	test_object("master:", "944c0f6e4dfa41595e6eb3ceecdb14f50fe18162");
	test_object("subtrees:", "ae90f12eea699729ed24555e40b9fd669da12a12");
	test_object("subtrees:ab", "f1425cef211cc08caa31e7b545ffb232acb098c3");

	/* Blobs */
	test_object("subtrees:ab/4.txt", "d6c93164c249c8000205dd4ec5cbca1b516d487f");
	test_object("subtrees:ab/de/fgh/1.txt", "1f67fc4386b2d171e0d21be1c447e12660561f9b");
	test_object("master:README", "a8233120f6ad708f843d861ce2b7228ec4e3dec6");
	test_object("master:new.txt", "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd");
	test_object(":/Merge", "a4a7dce85cf63874e984719f4fdd239f5145052f");
	test_object(":/one", "c47800c7266a2be04c571c04d5a6614691ea99bd");
	test_object(":/packed commit t", "41bc8c69075bbdb46c5c6f0566cc8cc5b46e8bd9");
	test_object("test/master^2:branch_file.txt", "45b983be36b73c0788dc9cbcb76cbb80fc7bb057");
}
