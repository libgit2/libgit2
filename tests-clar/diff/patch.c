#include "clar_libgit2.h"
#include "diff_helpers.h"
#include "repository.h"
#include "buf_text.h"

static git_repository *g_repo = NULL;

void test_diff_patch__initialize(void)
{
}

void test_diff_patch__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

#define EXPECTED_HEADER "diff --git a/subdir.txt b/subdir.txt\n" \
	"deleted file mode 100644\n" \
	"index e8ee89e..0000000\n" \
	"--- a/subdir.txt\n" \
	"+++ /dev/null\n"

#define EXPECTED_HUNK "@@ -1,2 +0,0 @@\n"

static int check_removal_cb(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *formatted_output,
	size_t output_len,
	void *payload)
{
	GIT_UNUSED(payload);
	GIT_UNUSED(output_len);

	switch (line_origin) {
	case GIT_DIFF_LINE_FILE_HDR:
		cl_assert_equal_s(EXPECTED_HEADER, formatted_output);
		cl_assert(range == NULL);
		goto check_delta;

	case GIT_DIFF_LINE_HUNK_HDR:
		cl_assert_equal_s(EXPECTED_HUNK, formatted_output);
		/* Fall through */

	case GIT_DIFF_LINE_CONTEXT:
	case GIT_DIFF_LINE_DELETION:
		goto check_range;

	default:
		/* unexpected code path */
		return -1;
	}

check_range:
	cl_assert(range != NULL);
	cl_assert_equal_i(1, range->old_start);
	cl_assert_equal_i(2, range->old_lines);
	cl_assert_equal_i(0, range->new_start);
	cl_assert_equal_i(0, range->new_lines);

check_delta:
	cl_assert_equal_s("subdir.txt", delta->old_file.path);
	cl_assert_equal_s("subdir.txt", delta->new_file.path);
	cl_assert_equal_i(GIT_DELTA_DELETED, delta->status);

	return 0;
}

void test_diff_patch__can_properly_display_the_removal_of_a_file(void)
{
	/*
	* $ git diff 26a125e..735b6a2
	* diff --git a/subdir.txt b/subdir.txt
	* deleted file mode 100644
	* index e8ee89e..0000000
	* --- a/subdir.txt
	* +++ /dev/null
	* @@ -1,2 +0,0 @@
	* -Is it a bird?
	* -Is it a plane?
	*/

	const char *one_sha = "26a125e";
	const char *another_sha = "735b6a2";
	git_tree *one, *another;
	git_diff_list *diff;

	g_repo = cl_git_sandbox_init("status");

	one = resolve_commit_oid_to_tree(g_repo, one_sha);
	another = resolve_commit_oid_to_tree(g_repo, another_sha);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, one, another, NULL));

	cl_git_pass(git_diff_print_patch(diff, check_removal_cb, NULL));

	git_diff_list_free(diff);

	git_tree_free(another);
	git_tree_free(one);
}

void test_diff_patch__to_string(void)
{
	const char *one_sha = "26a125e";
	const char *another_sha = "735b6a2";
	git_tree *one, *another;
	git_diff_list *diff;
	git_diff_patch *patch;
	char *text;
	const char *expected = "diff --git a/subdir.txt b/subdir.txt\ndeleted file mode 100644\nindex e8ee89e..0000000\n--- a/subdir.txt\n+++ /dev/null\n@@ -1,2 +0,0 @@\n-Is it a bird?\n-Is it a plane?\n";

	g_repo = cl_git_sandbox_init("status");

	one = resolve_commit_oid_to_tree(g_repo, one_sha);
	another = resolve_commit_oid_to_tree(g_repo, another_sha);

	cl_git_pass(git_diff_tree_to_tree(&diff, g_repo, one, another, NULL));

	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));

	cl_git_pass(git_diff_patch_to_str(&text, patch));

	cl_assert_equal_s(expected, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);
	git_tree_free(another);
	git_tree_free(one);
}

void test_diff_patch__hunks_have_correct_line_numbers(void)
{
	git_config *cfg;
	git_tree *head;
	git_diff_options opt = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *diff;
	git_diff_patch *patch;
	const git_diff_delta *delta;
	const git_diff_range *range;
	const char *hdr, *text;
	size_t hdrlen, hunklen, textlen;
	char origin;
	int oldno, newno;
	const char *new_content = "The Song of Seven Cities\n------------------------\n\nI WAS Lord of Cities very sumptuously builded.\nSeven roaring Cities paid me tribute from afar.\nIvory their outposts were--the guardrooms of them gilded,\nAnd garrisoned with Amazons invincible in war.\n\nThis is some new text;\nNot as good as the old text;\nBut here it is.\n\nSo they warred and trafficked only yesterday, my Cities.\nTo-day there is no mark or mound of where my Cities stood.\nFor the River rose at midnight and it washed away my Cities.\nThey are evened with Atlantis and the towns before the Flood.\n\nRain on rain-gorged channels raised the water-levels round them,\nFreshet backed on freshet swelled and swept their world from sight,\nTill the emboldened floods linked arms and, flashing forward, drowned them--\nDrowned my Seven Cities and their peoples in one night!\n\nLow among the alders lie their derelict foundations,\nThe beams wherein they trusted and the plinths whereon they built--\nMy rulers and their treasure and their unborn populations,\nDead, destroyed, aborted, and defiled with mud and silt!\n\nAnother replacement;\nBreaking up the poem;\nGenerating some hunks.\n\nTo the sound of trumpets shall their seed restore my Cities\nWealthy and well-weaponed, that once more may I behold\nAll the world go softly when it walks before my Cities,\nAnd the horses and the chariots fleeing from them as of old!\n\n  -- Rudyard Kipling\n";

	g_repo = cl_git_sandbox_init("renames");

	cl_git_pass(git_config_new(&cfg));
	git_repository_set_config(g_repo, cfg);

	cl_git_rewritefile("renames/songof7cities.txt", new_content);

	cl_git_pass(git_repository_head_tree(&head, g_repo));

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, head, &opt));

	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, &delta, diff, 0));

	cl_assert_equal_i(GIT_DELTA_MODIFIED, (int)delta->status);
	cl_assert_equal_i(2, (int)git_diff_patch_num_hunks(patch));

	/* check hunk 0 */

	cl_git_pass(
		git_diff_patch_get_hunk(&range, &hdr, &hdrlen, &hunklen, patch, 0));

	cl_assert_equal_i(18, (int)hunklen);

	cl_assert_equal_i(6, (int)range->old_start);
	cl_assert_equal_i(15, (int)range->old_lines);
	cl_assert_equal_i(6, (int)range->new_start);
	cl_assert_equal_i(9, (int)range->new_lines);

	cl_assert_equal_i(18, (int)git_diff_patch_num_lines_in_hunk(patch, 0));

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 0, 0));
	cl_assert_equal_i(GIT_DIFF_LINE_CONTEXT, (int)origin);
	cl_assert(strncmp("Ivory their outposts were--the guardrooms of them gilded,\n", text, textlen) == 0);
	cl_assert_equal_i(6, oldno);
	cl_assert_equal_i(6, newno);

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 0, 3));
	cl_assert_equal_i(GIT_DIFF_LINE_DELETION, (int)origin);
	cl_assert(strncmp("All the world went softly when it walked before my Cities--\n", text, textlen) == 0);
	cl_assert_equal_i(9, oldno);
	cl_assert_equal_i(-1, newno);

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 0, 12));
	cl_assert_equal_i(GIT_DIFF_LINE_ADDITION, (int)origin);
	cl_assert(strncmp("This is some new text;\n", text, textlen) == 0);
	cl_assert_equal_i(-1, oldno);
	cl_assert_equal_i(9, newno);

	/* check hunk 1 */

	cl_git_pass(
		git_diff_patch_get_hunk(&range, &hdr, &hdrlen, &hunklen, patch, 1));

	cl_assert_equal_i(18, (int)hunklen);

	cl_assert_equal_i(31, (int)range->old_start);
	cl_assert_equal_i(15, (int)range->old_lines);
	cl_assert_equal_i(25, (int)range->new_start);
	cl_assert_equal_i(9, (int)range->new_lines);

	cl_assert_equal_i(18, (int)git_diff_patch_num_lines_in_hunk(patch, 1));

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 1, 0));
	cl_assert_equal_i(GIT_DIFF_LINE_CONTEXT, (int)origin);
	cl_assert(strncmp("My rulers and their treasure and their unborn populations,\n", text, textlen) == 0);
	cl_assert_equal_i(31, oldno);
	cl_assert_equal_i(25, newno);

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 1, 3));
	cl_assert_equal_i(GIT_DIFF_LINE_DELETION, (int)origin);
	cl_assert(strncmp("The Daughters of the Palace whom they cherished in my Cities,\n", text, textlen) == 0);
	cl_assert_equal_i(34, oldno);
	cl_assert_equal_i(-1, newno);

	cl_git_pass(git_diff_patch_get_line_in_hunk(
		&origin, &text, &textlen, &oldno, &newno, patch, 1, 12));
	cl_assert_equal_i(GIT_DIFF_LINE_ADDITION, (int)origin);
	cl_assert(strncmp("Another replacement;\n", text, textlen) == 0);
	cl_assert_equal_i(-1, oldno);
	cl_assert_equal_i(28, newno);

	git_diff_patch_free(patch);
	git_diff_list_free(diff);
	git_tree_free(head);
}

static void check_single_patch_stats(
	git_repository *repo, size_t hunks, size_t adds, size_t dels)
{
	git_diff_list *diff;
	git_diff_patch *patch;
	const git_diff_delta *delta;
	size_t actual_adds, actual_dels;

	cl_git_pass(git_diff_index_to_workdir(&diff, repo, NULL, NULL));

	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, &delta, diff, 0));
	cl_assert_equal_i(GIT_DELTA_MODIFIED, (int)delta->status);

	cl_assert_equal_i((int)hunks, (int)git_diff_patch_num_hunks(patch));

	cl_git_pass(
		git_diff_patch_line_stats(NULL, &actual_adds, &actual_dels, patch));

	cl_assert_equal_sz(adds, actual_adds);
	cl_assert_equal_sz(dels, actual_dels);

	git_diff_patch_free(patch);
	git_diff_list_free(diff);
}

void test_diff_patch__line_counts_with_eofnl(void)
{
	git_config *cfg;
	git_buf content = GIT_BUF_INIT;
	const char *end;
	git_index *index;

	g_repo = cl_git_sandbox_init("renames");

	cl_git_pass(git_config_new(&cfg));
	git_repository_set_config(g_repo, cfg);

	cl_git_pass(git_futils_readbuffer(&content, "renames/songof7cities.txt"));

	/* remove first line */

	end = git_buf_cstr(&content) + git_buf_find(&content, '\n') + 1;
	git_buf_consume(&content, end);
	cl_git_rewritefile("renames/songof7cities.txt", content.ptr);

	check_single_patch_stats(g_repo, 1, 0, 1);

	/* remove trailing whitespace */

	git_buf_rtrim(&content);
	cl_git_rewritefile("renames/songof7cities.txt", content.ptr);

	check_single_patch_stats(g_repo, 2, 1, 2);

	/* add trailing whitespace */

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, "songof7cities.txt"));
	cl_git_pass(git_index_write(index));
	git_index_free(index);

	cl_git_pass(git_buf_putc(&content, '\n'));
	cl_git_rewritefile("renames/songof7cities.txt", content.ptr);

	check_single_patch_stats(g_repo, 1, 1, 1);

	git_buf_free(&content);
}
