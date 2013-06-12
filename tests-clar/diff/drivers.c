#include "clar_libgit2.h"
#include "diff_helpers.h"
#include "repository.h"
#include "diff_driver.h"

static git_repository *g_repo = NULL;
static git_config *g_cfg = NULL;

void test_diff_drivers__initialize(void)
{
}

void test_diff_drivers__cleanup(void)
{
	git_config_free(g_cfg);
	g_cfg = NULL;

	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_diff_drivers__patterns(void)
{
	const char *one_sha = "19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13";
	git_tree *one;
	git_diff_list *diff;
	git_diff_patch *patch;
	char *text;
	const char *expected0 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\n--- a/untimely.txt\n+++ b/untimely.txt\n@@ -22,3 +22,5 @@ Comes through the blood of the vanguards who\n   dreamed--too soon--it had sounded.\r\n \r\n                 -- Rudyard Kipling\r\n+\r\n+Some new stuff\r\n";
	const char *expected1 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\nBinary files a/untimely.txt and b/untimely.txt differ\n";
	const char *expected2 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\n--- a/untimely.txt\n+++ b/untimely.txt\n@@ -22,3 +22,5 @@ Heaven delivers on earth the Hour that cannot be\n   dreamed--too soon--it had sounded.\r\n \r\n                 -- Rudyard Kipling\r\n+\r\n+Some new stuff\r\n";

	g_repo = cl_git_sandbox_init("renames");

	one = resolve_commit_oid_to_tree(g_repo, one_sha);

	/* no diff */

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(0, (int)git_diff_num_deltas(diff));
	git_diff_list_free(diff);

	/* default diff */

	cl_git_append2file("renames/untimely.txt", "\r\nSome new stuff\r\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));
	cl_git_pass(git_diff_patch_to_str(&text, patch));
	cl_assert_equal_s(expected0, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);

	/* attribute diff set to false */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt -diff\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));
	cl_git_pass(git_diff_patch_to_str(&text, patch));
	cl_assert_equal_s(expected1, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);

	/* attribute diff set to unconfigured value (should use default) */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt diff=kipling0\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));
	cl_git_pass(git_diff_patch_to_str(&text, patch));
	cl_assert_equal_s(expected0, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);

	/* let's define that driver */

	cl_git_pass(git_repository_config(&g_cfg, g_repo));
	cl_git_pass(git_config_set_bool(g_cfg, "diff.kipling0.binary", 1));

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));
	cl_git_pass(git_diff_patch_to_str(&text, patch));
	cl_assert_equal_s(expected1, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);

	/* let's use a real driver with some regular expressions */

	git_diff_driver_registry_free(g_repo->diff_drivers);
	g_repo->diff_drivers = NULL;

	cl_git_pass(git_repository_config(&g_cfg, g_repo));
	cl_git_pass(git_config_set_bool(g_cfg, "diff.kipling0.binary", 0));
	cl_git_pass(git_config_set_string(g_cfg, "diff.kipling0.xfuncname", "^H"));

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_diff_get_patch(&patch, NULL, diff, 0));
	cl_git_pass(git_diff_patch_to_str(&text, patch));
	cl_assert_equal_s(expected2, text);

	git__free(text);
	git_diff_patch_free(patch);
	git_diff_list_free(diff);

	git_tree_free(one);
}

