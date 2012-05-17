#include "clar_libgit2.h"

#include "tag.h"

static const char *tag1_id = "b25fa35b38051e4ae45d4222e795f9df2e43f1d1";
static const char *tag2_id = "7b4384978d2493e851f9cca7858815fac9b10980";
static const char *tagged_commit = "e90810b8df3e80c413d903f631643c716887138d";
static const char *bad_tag_id = "eda9f45a2a98d4c17a09d681d88569fa4ea91755";
static const char *badly_tagged_commit = "e90810b8df3e80c413d903f631643c716887138d";

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
void test_object_tag_read__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tag_read__cleanup(void)
{
   cl_git_sandbox_cleanup();
}


void test_object_tag_read__parse(void)
{
   // read and parse a tag from the repository
   git_tag *tag1, *tag2;
   git_commit *commit;
   git_oid id1, id2, id_commit;

   git_oid_fromstr(&id1, tag1_id);
   git_oid_fromstr(&id2, tag2_id);
   git_oid_fromstr(&id_commit, tagged_commit);

   cl_git_pass(git_tag_lookup(&tag1, g_repo, &id1));

   cl_assert_equal_s(git_tag_name(tag1), "test");
   cl_assert(git_tag_type(tag1) == GIT_OBJ_TAG);

   cl_git_pass(git_tag_target((git_object **)&tag2, tag1));
   cl_assert(tag2 != NULL);

   cl_assert(git_oid_cmp(&id2, git_tag_id(tag2)) == 0);

   cl_git_pass(git_tag_target((git_object **)&commit, tag2));
   cl_assert(commit != NULL);

   cl_assert(git_oid_cmp(&id_commit, git_commit_id(commit)) == 0);

   git_tag_free(tag1);
   git_tag_free(tag2);
   git_commit_free(commit);
}

void test_object_tag_read__list(void)
{
   // list all tag names from the repository
   git_strarray tag_list;

   cl_git_pass(git_tag_list(&tag_list, g_repo));

   cl_assert(tag_list.count == 3);

   git_strarray_free(&tag_list);
}

void test_object_tag_read__list_pattern(void)
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

void test_object_tag_read__parse_without_tagger(void)
{
   // read and parse a tag without a tagger field
   git_repository *bad_tag_repo;
   git_tag *bad_tag;
   git_commit *commit;
   git_oid id, id_commit;

   // TODO: This is a little messy
   cl_git_pass(git_repository_open(&bad_tag_repo, cl_fixture("bad_tag.git")));

   git_oid_fromstr(&id, bad_tag_id);
   git_oid_fromstr(&id_commit, badly_tagged_commit);

   cl_git_pass(git_tag_lookup(&bad_tag, bad_tag_repo, &id));
   cl_assert(bad_tag != NULL);

   cl_assert_equal_s(git_tag_name(bad_tag), "e90810b");
   cl_assert(git_oid_cmp(&id, git_tag_id(bad_tag)) == 0);
   cl_assert(bad_tag->tagger == NULL);

   cl_git_pass(git_tag_target((git_object **)&commit, bad_tag));
   cl_assert(commit != NULL);

   cl_assert(git_oid_cmp(&id_commit, git_commit_id(commit)) == 0);

   git_tag_free(bad_tag);
   git_commit_free(commit);
   git_repository_free(bad_tag_repo);
}
