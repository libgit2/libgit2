#include "clar_libgit2.h"

#include "git2/revparse.h"

static git_repository *g_repo;
static git_object *g_obj;



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
   g_repo = cl_git_sandbox_init("testrepo.git");
}

void test_refs_revparse__cleanup(void)
{
   cl_git_sandbox_cleanup();
   g_obj = NULL;
}


void test_refs_revparse__nonexistant_object(void)
{
   cl_git_fail(git_revparse_single(&g_obj, g_repo, "this doesn't exist"));
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
   test_object("HEAD@{10 years ago}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
   test_object("HEAD@{1 second}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
   test_object("master@{2012-4-30 10:23:20 -0800}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
   test_object("master@{2012-4-30 10:24 -0800}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
   test_object("master@{2012-4-30 16:24 -0200}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");
   test_object("master@{1335806600}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
   test_object("master@{1335816640}", "a65fedf39aefe402d3bb6e24df4d4f5fe4547750");

   /* Core git gives a65fedf, because they don't take time zones into account. */
   test_object("master@{1335806640}", "be3563ae3f795b2b4353bcce3a527ad0a4f7f644");
}

void test_refs_revparse__colon(void)
{
   cl_git_fail(git_revparse_single(&g_obj, g_repo, ":/foo"));
   cl_git_fail(git_revparse_single(&g_obj, g_repo, ":2:README"));

   test_object("subtrees:ab/4.txt", "d6c93164c249c8000205dd4ec5cbca1b516d487f");
   test_object("subtrees:ab/de/fgh/1.txt", "1f67fc4386b2d171e0d21be1c447e12660561f9b");
   test_object("master:README", "a8233120f6ad708f843d861ce2b7228ec4e3dec6");
   test_object("master:new.txt", "a71586c1dfe8a71c6cbf6c129f404c5642ff31bd");
}
