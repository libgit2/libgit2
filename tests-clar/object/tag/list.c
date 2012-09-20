#include "clar_libgit2.h"

#include "tag.h"

static git_repository *g_repo;

// Helpers
static void ensure_tag_pattern_match(git_repository *repo,
                                     const char *pattern,
                                     const size_t expected_matches)
{
   git_strarray tag_list;
   int error = 0;

   if ((error = git_tag_list_match(&tag_list, pattern, repo)) < 0)
      goto exit;

   if (tag_list.count != expected_matches)
      error = GIT_ERROR;

exit:
   git_strarray_free(&tag_list);
   cl_git_pass(error);
}


// Fixture setup and teardown
void test_object_tag_list__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tag_list__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

void test_object_tag_list__list_all(void)
{
   // list all tag names from the repository
   git_strarray tag_list;

   cl_git_pass(git_tag_list(&tag_list, g_repo));

   cl_assert(tag_list.count == 3);

   git_strarray_free(&tag_list);
}

void test_object_tag_list__list_by_pattern(void)
{
   // list all tag names from the repository matching a specified pattern
   ensure_tag_pattern_match(g_repo, "", 3);
   ensure_tag_pattern_match(g_repo, "*", 3);
   ensure_tag_pattern_match(g_repo, "t*", 1);
   ensure_tag_pattern_match(g_repo, "*b", 2);
   ensure_tag_pattern_match(g_repo, "e", 0);
   ensure_tag_pattern_match(g_repo, "e90810b", 1);
   ensure_tag_pattern_match(g_repo, "e90810[ab]", 1);
}
