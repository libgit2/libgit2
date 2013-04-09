#include "clar_libgit2.h"
#include "repository.h"
#include "../submodule/submodule_helpers.h"

static git_repository *g_repo = NULL;

static void setup_submodules(void)
{
	g_repo = cl_git_sandbox_init("submodules");
	cl_fixture_sandbox("testrepo.git");
	rewrite_gitmodules(git_repository_workdir(g_repo));
	p_rename("submodules/testrepo/.gitted", "submodules/testrepo/.git");
}

static void setup_submodules2(void)
{
	g_repo = cl_git_sandbox_init("submod2");

	cl_fixture_sandbox("submod2_target");
	p_rename("submod2_target/.gitted", "submod2_target/.git");

	rewrite_gitmodules(git_repository_workdir(g_repo));
	p_rename("submod2/not-submodule/.gitted", "submod2/not-submodule/.git");
	p_rename("submod2/not/.gitted", "submod2/not/.git");
}

void test_diff_submodules__initialize(void)
{
}

void test_diff_submodules__cleanup(void)
{
	cl_git_sandbox_cleanup();

	cl_fixture_cleanup("testrepo.git");
	cl_fixture_cleanup("submod2_target");
}

static void check_diff_patches(git_diff_list *diff, const char **expected)
{
	const git_diff_delta *delta;
	git_diff_patch *patch = NULL;
	size_t d, num_d = git_diff_num_deltas(diff);
	char *patch_text;

	for (d = 0; d < num_d; ++d, git_diff_patch_free(patch)) {
		cl_git_pass(git_diff_get_patch(&patch, &delta, diff, d));

		if (delta->status == GIT_DELTA_UNMODIFIED)
			continue;

		if (expected[d] && !strcmp(expected[d], "<SKIP>"))
			continue;
		if (expected[d] && !strcmp(expected[d], "<END>"))
			cl_assert(0);

		cl_git_pass(git_diff_patch_to_str(&patch_text, patch));

		cl_assert_equal_s(expected[d], patch_text);
		git__free(patch_text);
	}

	cl_assert(expected[d] && !strcmp(expected[d], "<END>"));
}

void test_diff_submodules__unmodified_submodule(void)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *diff = NULL;
	static const char *expected[] = {
		"<SKIP>", /* .gitmodules */
		NULL, /* added */
		NULL, /* ignored */
		"diff --git a/modified b/modified\nindex 092bfb9..452216e 100644\n--- a/modified\n+++ b/modified\n@@ -1 +1,2 @@\n-yo\n+changed\n+\n", /* modified */
		NULL, /* testrepo.git */
		NULL, /* unmodified */
		NULL, /* untracked */
		"<END>"
	};

	setup_submodules();

	opts.flags = GIT_DIFF_INCLUDE_IGNORED |
		GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_INCLUDE_UNMODIFIED;

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
	check_diff_patches(diff, expected);
	git_diff_list_free(diff);
}

void test_diff_submodules__dirty_submodule(void)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *diff = NULL;
	static const char *expected[] = {
		"<SKIP>", /* .gitmodules */
		NULL, /* added */
		NULL, /* ignored */
		"diff --git a/modified b/modified\nindex 092bfb9..452216e 100644\n--- a/modified\n+++ b/modified\n@@ -1 +1,2 @@\n-yo\n+changed\n+\n", /* modified */
		"diff --git a/testrepo b/testrepo\nindex a65fedf..a65fedf 160000\n--- a/testrepo\n+++ b/testrepo\n@@ -1 +1 @@\n-Subproject commit a65fedf39aefe402d3bb6e24df4d4f5fe4547750\n+Subproject commit a65fedf39aefe402d3bb6e24df4d4f5fe4547750-dirty\n", /* testrepo.git */
		NULL, /* unmodified */
		NULL, /* untracked */
		"<END>"
	};

	setup_submodules();

	cl_git_rewritefile("submodules/testrepo/README", "heyheyhey");
	cl_git_mkfile("submodules/testrepo/all_new.txt", "never seen before");

	opts.flags = GIT_DIFF_INCLUDE_IGNORED |
		GIT_DIFF_INCLUDE_UNTRACKED |
		GIT_DIFF_INCLUDE_UNMODIFIED;

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
	check_diff_patches(diff, expected);
	git_diff_list_free(diff);
}

void test_diff_submodules__submod2_index_to_wd(void)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *diff = NULL;
	static const char *expected[] = {
		"<SKIP>", /* .gitmodules */
		NULL, /* not-submodule */
		NULL, /* not */
		"diff --git a/sm_changed_file b/sm_changed_file\nindex 4800958..4800958 160000\n--- a/sm_changed_file\n+++ b/sm_changed_file\n@@ -1 +1 @@\n-Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n+Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0-dirty\n", /* sm_changed_file */
		"diff --git a/sm_changed_head b/sm_changed_head\nindex 4800958..3d9386c 160000\n--- a/sm_changed_head\n+++ b/sm_changed_head\n@@ -1 +1 @@\n-Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n+Subproject commit 3d9386c507f6b093471a3e324085657a3c2b4247\n", /* sm_changed_head */
		"diff --git a/sm_changed_index b/sm_changed_index\nindex 4800958..4800958 160000\n--- a/sm_changed_index\n+++ b/sm_changed_index\n@@ -1 +1 @@\n-Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n+Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0-dirty\n", /* sm_changed_index */
		"diff --git a/sm_changed_untracked_file b/sm_changed_untracked_file\nindex 4800958..4800958 160000\n--- a/sm_changed_untracked_file\n+++ b/sm_changed_untracked_file\n@@ -1 +1 @@\n-Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n+Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0-dirty\n", /* sm_changed_untracked_file */
		"diff --git a/sm_missing_commits b/sm_missing_commits\nindex 4800958..5e49635 160000\n--- a/sm_missing_commits\n+++ b/sm_missing_commits\n@@ -1 +1 @@\n-Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n+Subproject commit 5e4963595a9774b90524d35a807169049de8ccad\n", /* sm_missing_commits */
		"<END>"
	};

	setup_submodules2();

	opts.flags = GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_index_to_workdir(&diff, g_repo, NULL, &opts));
	check_diff_patches(diff, expected);
	git_diff_list_free(diff);
}

void test_diff_submodules__submod2_head_to_index(void)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_tree *head;
	git_diff_list *diff = NULL;
	static const char *expected[] = {
		"<SKIP>", /* .gitmodules */
		"diff --git a/sm_added_and_uncommited b/sm_added_and_uncommited\nnew file mode 160000\nindex 0000000..4800958\n--- /dev/null\n+++ b/sm_added_and_uncommited\n@@ -0,0 +1 @@\n+Subproject commit 480095882d281ed676fe5b863569520e54a7d5c0\n", /* sm_added_and_uncommited */
		"<END>"
	};

	setup_submodules2();

	cl_git_pass(git_repository_head_tree(&head, g_repo));

	opts.flags = GIT_DIFF_INCLUDE_UNTRACKED;

	cl_git_pass(git_diff_tree_to_index(&diff, g_repo, head, NULL, &opts));
	check_diff_patches(diff, expected);
	git_diff_list_free(diff);

	git_tree_free(head);
}
