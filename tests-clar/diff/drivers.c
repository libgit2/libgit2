#include "clar_libgit2.h"
#include "diff_helpers.h"
#include "repository.h"
#include "diff_driver.h"

static git_repository *g_repo = NULL;

void test_diff_drivers__initialize(void)
{
}

void test_diff_drivers__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

void test_diff_drivers__patterns(void)
{
	git_config *cfg;
	const char *one_sha = "19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13";
	git_tree *one;
	git_diff *diff;
	git_patch *patch;
	char *text;
	const char *expected0 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\n--- a/untimely.txt\n+++ b/untimely.txt\n@@ -22,3 +22,5 @@ Comes through the blood of the vanguards who\n   dreamed--too soon--it had sounded.\r\n \r\n                 -- Rudyard Kipling\r\n+\r\n+Some new stuff\r\n";
	const char *expected1 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\nBinary files a/untimely.txt and b/untimely.txt differ\n";
	const char *expected2 = "diff --git a/untimely.txt b/untimely.txt\nindex 9a69d96..57fd0cf 100644\n--- a/untimely.txt\n+++ b/untimely.txt\n@@ -22,3 +22,5 @@ Heaven delivers on earth the Hour that cannot be\n   dreamed--too soon--it had sounded.\r\n \r\n                 -- Rudyard Kipling\r\n+\r\n+Some new stuff\r\n";

	g_repo = cl_git_sandbox_init("renames");

	one = resolve_commit_oid_to_tree(g_repo, one_sha);

	/* no diff */

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(0, (int)git_diff_num_deltas(diff));
	git_diff_free(diff);

	/* default diff */

	cl_git_append2file("renames/untimely.txt", "\r\nSome new stuff\r\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&text, patch));
	cl_assert_equal_s(expected0, text);

	git__free(text);
	git_patch_free(patch);
	git_diff_free(diff);

	/* attribute diff set to false */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt -diff\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&text, patch));
	cl_assert_equal_s(expected1, text);

	git__free(text);
	git_patch_free(patch);
	git_diff_free(diff);

	/* attribute diff set to unconfigured value (should use default) */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt diff=kipling0\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&text, patch));
	cl_assert_equal_s(expected0, text);

	git__free(text);
	git_patch_free(patch);
	git_diff_free(diff);

	/* let's define that driver */

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "diff.kipling0.binary", 1));
	git_config_free(cfg);

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&text, patch));
	cl_assert_equal_s(expected1, text);

	git__free(text);
	git_patch_free(patch);
	git_diff_free(diff);

	/* let's use a real driver with some regular expressions */

	git_diff_driver_registry_free(g_repo->diff_drivers);
	g_repo->diff_drivers = NULL;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "diff.kipling0.binary", 0));
	cl_git_pass(git_config_set_string(cfg, "diff.kipling0.xfuncname", "^H"));
	git_config_free(cfg);

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&text, patch));
	cl_assert_equal_s(expected2, text);

	git__free(text);
	git_patch_free(patch);
	git_diff_free(diff);

	git_tree_free(one);
}

void test_diff_drivers__long_lines(void)
{
	const char *base = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non nisi ligula. Ut viverra enim sed lobortis suscipit.\nPhasellus eget erat odio. Praesent at est iaculis, ultricies augue vel, dignissim risus. Suspendisse at nisi quis turpis fringilla rutrum id sit amet nulla.\nNam eget dolor fermentum, aliquet nisl at, convallis tellus. Pellentesque rhoncus erat enim, id porttitor elit euismod quis.\nMauris sollicitudin magna odio, non egestas libero vehicula ut. Etiam et quam velit. Fusce eget libero rhoncus, ultricies felis sit amet, egestas purus.\nAliquam in semper tellus. Pellentesque adipiscing rutrum velit, quis malesuada lacus consequat eget.\n";
	git_index *idx;
	git_diff *diff;
	git_patch *patch;
	char *actual;
	const char *expected = "diff --git a/longlines.txt b/longlines.txt\nindex c1ce6ef..0134431 100644\n--- a/longlines.txt\n+++ b/longlines.txt\n@@ -3,3 +3,5 @@ Phasellus eget erat odio. Praesent at est iaculis, ultricies augue vel, dignissi\n Nam eget dolor fermentum, aliquet nisl at, convallis tellus. Pellentesque rhoncus erat enim, id porttitor elit euismod quis.\n Mauris sollicitudin magna odio, non egestas libero vehicula ut. Etiam et quam velit. Fusce eget libero rhoncus, ultricies felis sit amet, egestas purus.\n Aliquam in semper tellus. Pellentesque adipiscing rutrum velit, quis malesuada lacus consequat eget.\n+newline\n+newline\n";

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile("empty_standard_repo/longlines.txt", base);
	cl_git_pass(git_repository_index(&idx, g_repo));
	cl_git_pass(git_index_add_bypath(idx, "longlines.txt"));
	cl_git_pass(git_index_write(idx));
	git_index_free(idx);

	cl_git_append2file("empty_standard_repo/longlines.txt", "newline\nnewline\n");

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, NULL));
	cl_assert_equal_sz(1, git_diff_num_deltas(diff));
	cl_git_pass(git_patch_from_diff(&patch, NULL, diff, 0));
	cl_git_pass(git_patch_to_str(&actual, patch));

	/* if chmod not supported, overwrite mode bits since anything is possible */
	if (!cl_is_chmod_supported()) {
		size_t actual_len = strlen(actual);
		if (actual_len > 72 && memcmp(&actual[66], "100644", 6) != 0)
			memcpy(&actual[66], "100644", 6);
	}

	cl_assert_equal_s(expected, actual);

	free(actual);
	git_patch_free(patch);
	git_diff_free(diff);
}

