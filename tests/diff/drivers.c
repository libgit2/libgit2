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

static void overwrite_filemode(const char *expected, git_buf *actual)
{
	size_t offset;
	char *found;

	found = strstr(expected, "100644");
	if (!found)
		return;

	offset = ((const char *)found) - expected;
	if (actual->size < offset + 6)
		return;

	if (memcmp(&actual->ptr[offset], "100644", 6) != 0)
		memcpy(&actual->ptr[offset], "100644", 6);
}

void test_diff_drivers__patterns(void)
{
	git_config *cfg;
	const char *one_sha = "19dd32dfb1520a64e5bbaae8dce6ef423dfa2f13";
	git_tree *one;
	git_diff *diff;
	git_patch *patch;
	git_buf actual = GIT_BUF_INIT;
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

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));
	cl_assert_equal_s(expected0, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);

	/* attribute diff set to false */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt -diff\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));
	cl_assert_equal_s(expected1, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);

	/* attribute diff set to unconfigured value (should use default) */

	cl_git_rewritefile("renames/.gitattributes", "untimely.txt diff=kipling0\n");

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));
	cl_assert_equal_s(expected0, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);

	/* let's define that driver */

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "diff.kipling0.binary", 1));
	git_config_free(cfg);

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));
	cl_assert_equal_s(expected1, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);

	/* let's use a real driver with some regular expressions */

	git_diff_driver_registry_free(g_repo->diff_drivers);
	g_repo->diff_drivers = NULL;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "diff.kipling0.binary", 0));
	cl_git_pass(git_config_set_string(cfg, "diff.kipling0.xfuncname", "^H.*$"));
	git_config_free(cfg);

	cl_git_pass(git_diff_tree_to_workdir(&diff, g_repo, one, NULL));
	cl_assert_equal_i(1, (int)git_diff_num_deltas(diff));

	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));
	cl_assert_equal_s(expected2, actual.ptr);

	git_buf_free(&actual);
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
	git_buf actual = GIT_BUF_INIT;
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
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));

	/* if chmod not supported, overwrite mode bits since anything is possible */
	overwrite_filemode(expected, &actual);

	cl_assert_equal_s(expected, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);
}

void test_diff_drivers__builtins(void)
{
	git_index *idx;
	git_diff *diff;
	git_patch *patch;
	git_buf actual = GIT_BUF_INIT;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	const char *base =
		"<html>\n<body>\n"
		"  <h1 id=\"first section\">\n  <ol>\n    <li>item 1.1</li>\n    <li>item 1.2</li>\n    <li>item 1.3</li>\n    <li>item 1.4</li>\n    <li>item 1.5</li>\n    <li>item 1.6</li>\n    <li>item 1.7</li>\n    <li>item 1.8</li>\n    <li>item 1.9</li>\n  </ol>\n  </h1>\n"
		"  <h1 id=\"second section\">\n  <ol>\n    <li>item 2.1</li>\n    <li>item 2.2</li>\n    <li>item 2.3</li>\n    <li>item 2.4</li>\n    <li>item 2.5</li>\n    <li>item 2.6</li>\n    <li>item 2.7</li>\n    <li>item 2.8</li>\n  </ol>\n  </h1>\n"
		"  <h1 id=\"third section\">\n  <ol>\n    <li>item 3.1</li>\n    <li>item 3.2</li>\n    <li>item 3.3</li>\n    <li>item 3.4</li>\n    <li>item 3.5</li>\n    <li>item 3.6</li>\n    <li>item 3.7</li>\n    <li>item 3.8</li>\n  </ol>\n  </h1>\n"
		"</body></html>\n";
	const char *modified =
		"<html>\n<body>\n"
		"  <h1 id=\"first section\">\n  <ol>\n    <li>item 1.1</li>\n    <li>item 1.2 changed</li>\n    <li>item 1.3 changed</li>\n    <li>item 1.4</li>\n    <li>item 1.5</li>\n    <li>item 1.6</li>\n    <li>item 1.7</li>\n    <li>item 1.8</li>\n    <li>item 1.9</li>\n  <li>item 1.10 added</li>\n  </ol>\n  </h1>\n"
		"  <h1 id=\"second section\">\n  <ol>\n    <li>item 2.1</li>\n    <li>item 2.2</li>\n    <li>item 2.3</li>\n    <li>item 2.4</li>\n    <li>item 2.5</li>\n    <li>item 2.6</li>\n    <li>item 2.7 changed</li>\n    <li>item 2.7.1 added</li>\n    <li>item 2.8</li>\n  </ol>\n  </h1>\n"
		"  <h1 id=\"third section\">\n  <ol>\n    <li>item 3.1</li>\n    <li>item 3.2</li>\n    <li>item 3.3</li>\n    <li>item 3.4</li>\n    <li>item 3.5</li>\n    <li>item 3.6</li>\n  </ol>\n  </h1>\n"
		"</body></html>\n";
	const char *expected_nodriver =
		"diff --git a/file.html b/file.html\nindex 97b34db..c7dbed3 100644\n--- a/file.html\n+++ b/file.html\n@@ -5,4 +5,4 @@\n     <li>item 1.1</li>\n-    <li>item 1.2</li>\n-    <li>item 1.3</li>\n+    <li>item 1.2 changed</li>\n+    <li>item 1.3 changed</li>\n     <li>item 1.4</li>\n@@ -13,2 +13,3 @@\n     <li>item 1.9</li>\n+  <li>item 1.10 added</li>\n   </ol>\n@@ -23,3 +24,4 @@\n     <li>item 2.6</li>\n-    <li>item 2.7</li>\n+    <li>item 2.7 changed</li>\n+    <li>item 2.7.1 added</li>\n     <li>item 2.8</li>\n@@ -35,4 +37,2 @@\n     <li>item 3.6</li>\n-    <li>item 3.7</li>\n-    <li>item 3.8</li>\n   </ol>\n";
	const char *expected_driver =
		"diff --git a/file.html b/file.html\nindex 97b34db..c7dbed3 100644\n--- a/file.html\n+++ b/file.html\n@@ -5,4 +5,4 @@ <h1 id=\"first section\">\n     <li>item 1.1</li>\n-    <li>item 1.2</li>\n-    <li>item 1.3</li>\n+    <li>item 1.2 changed</li>\n+    <li>item 1.3 changed</li>\n     <li>item 1.4</li>\n@@ -13,2 +13,3 @@ <h1 id=\"first section\">\n     <li>item 1.9</li>\n+  <li>item 1.10 added</li>\n   </ol>\n@@ -23,3 +24,4 @@ <h1 id=\"second section\">\n     <li>item 2.6</li>\n-    <li>item 2.7</li>\n+    <li>item 2.7 changed</li>\n+    <li>item 2.7.1 added</li>\n     <li>item 2.8</li>\n@@ -35,4 +37,2 @@ <h1 id=\"third section\">\n     <li>item 3.6</li>\n-    <li>item 3.7</li>\n-    <li>item 3.8</li>\n   </ol>\n";

	g_repo = cl_git_sandbox_init("empty_standard_repo");

	cl_git_mkfile("empty_standard_repo/file.html", base);
	cl_git_pass(git_repository_index(&idx, g_repo));
	cl_git_pass(git_index_add_bypath(idx, "file.html"));
	cl_git_pass(git_index_write(idx));
	git_index_free(idx);

	cl_git_rewritefile("empty_standard_repo/file.html", modified);

	/* do diff with no special driver */

	opts.interhunk_lines = 1;
	opts.context_lines = 1;

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
	cl_assert_equal_sz(1, git_diff_num_deltas(diff));
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));

	overwrite_filemode(expected_nodriver, &actual);

	cl_assert_equal_s(expected_nodriver, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);

	/* do diff with HTML driver */

	cl_git_mkfile("empty_standard_repo/.gitattributes", "*.html diff=html\n");

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
	cl_assert_equal_sz(1, git_diff_num_deltas(diff));
	cl_git_pass(git_patch_from_diff(&patch, diff, 0));
	cl_git_pass(git_patch_to_buf(&actual, patch));

	overwrite_filemode(expected_driver, &actual);

	cl_assert_equal_s(expected_driver, actual.ptr);

	git_buf_free(&actual);
	git_patch_free(patch);
	git_diff_free(diff);
}
