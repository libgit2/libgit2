#include "clar_libgit2.h"
#include "buffer.h"
#include "path.h"
#include "posix.h"
#include "status_helpers.h"
#include "../submodule/submodule_helpers.h"

static git_repository *g_repo = NULL;

void test_status_submodules__initialize(void)
{
	g_repo = cl_git_sandbox_init("submodules");

	cl_fixture_sandbox("testrepo.git");

	rewrite_gitmodules(git_repository_workdir(g_repo));

	p_rename("submodules/testrepo/.gitted", "submodules/testrepo/.git");
}

void test_status_submodules__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("testrepo.git");
}

void test_status_submodules__api(void)
{
	git_submodule *sm;

	cl_assert(git_submodule_lookup(NULL, g_repo, "nonexistent") == GIT_ENOTFOUND);

	cl_assert(git_submodule_lookup(NULL, g_repo, "modified") == GIT_ENOTFOUND);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_assert(sm != NULL);
	cl_assert_equal_s("testrepo", git_submodule_name(sm));
	cl_assert_equal_s("testrepo", git_submodule_path(sm));
}

void test_status_submodules__0(void)
{
	int counts = 0;

	cl_assert(git_path_isdir("submodules/.git"));
	cl_assert(git_path_isdir("submodules/testrepo/.git"));
	cl_assert(git_path_isfile("submodules/.gitmodules"));

	cl_git_pass(
		git_status_foreach(g_repo, cb_status__count, &counts)
	);

	cl_assert_equal_i(6, counts);
}

static const char *expected_files[] = {
	".gitmodules",
	"added",
	"deleted",
	"ignored",
	"modified",
	"untracked"
};

static unsigned int expected_status[] = {
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_INDEX_NEW,
	GIT_STATUS_INDEX_DELETED,
	GIT_STATUS_IGNORED,
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_WT_NEW
};

static int cb_status__match(const char *p, unsigned int s, void *payload)
{
	status_entry_counts *counts = payload;
	int idx = counts->entry_count++;

	cl_assert_equal_s(counts->expected_paths[idx], p);
	cl_assert(counts->expected_statuses[idx] == s);

	return 0;
}

void test_status_submodules__1(void)
{
	status_entry_counts counts;

	cl_assert(git_path_isdir("submodules/.git"));
	cl_assert(git_path_isdir("submodules/testrepo/.git"));
	cl_assert(git_path_isfile("submodules/.gitmodules"));

	memset(&counts, 0, sizeof(counts));
	counts.expected_paths = expected_files;
	counts.expected_statuses = expected_status;

	cl_git_pass(
		git_status_foreach(g_repo, cb_status__match, &counts)
	);

	cl_assert_equal_i(6, counts.entry_count);
}

void test_status_submodules__single_file(void)
{
	unsigned int status = 0;
	cl_git_pass( git_status_file(&status, g_repo, "testrepo") );
	cl_assert(!status);
}

void test_status_submodules__moved_head(void)
{
	git_submodule *sm;
	git_repository *smrepo;
	git_oid oid;
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	status_entry_counts counts;
	static const char *expected_files_with_sub[] = {
		".gitmodules",
		"added",
		"deleted",
		"ignored",
		"modified",
		"testrepo",
		"untracked"
	};
	static unsigned int expected_status_with_sub[] = {
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_INDEX_NEW,
		GIT_STATUS_INDEX_DELETED,
		GIT_STATUS_IGNORED,
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_WT_NEW
	};

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_git_pass(git_submodule_open(&smrepo, sm));

	/* move submodule HEAD to c47800c7266a2be04c571c04d5a6614691ea99bd */
	cl_git_pass(
		git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_repository_set_head_detached(smrepo, &oid));

	/* first do a normal status, which should now include the submodule */

	memset(&counts, 0, sizeof(counts));
	counts.expected_paths = expected_files_with_sub;
	counts.expected_statuses = expected_status_with_sub;

	opts.flags = GIT_STATUS_OPT_DEFAULTS;

	cl_git_pass(
		git_status_foreach_ext(g_repo, &opts, cb_status__match, &counts));
	cl_assert_equal_i(7, counts.entry_count);

	/* try again with EXCLUDE_SUBMODULES which should skip it */

	memset(&counts, 0, sizeof(counts));
	counts.expected_paths = expected_files;
	counts.expected_statuses = expected_status;

	opts.flags = GIT_STATUS_OPT_DEFAULTS | GIT_STATUS_OPT_EXCLUDE_SUBMODULES;

	cl_git_pass(
		git_status_foreach_ext(g_repo, &opts, cb_status__match, &counts));
	cl_assert_equal_i(6, counts.entry_count);

	git_repository_free(smrepo);
}

void test_status_submodules__dirty_workdir_only(void)
{
	git_status_options opts = GIT_STATUS_OPTIONS_INIT;
	status_entry_counts counts;
	static const char *expected_files_with_sub[] = {
		".gitmodules",
		"added",
		"deleted",
		"ignored",
		"modified",
		"testrepo",
		"untracked"
	};
	static unsigned int expected_status_with_sub[] = {
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_INDEX_NEW,
		GIT_STATUS_INDEX_DELETED,
		GIT_STATUS_IGNORED,
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_WT_MODIFIED,
		GIT_STATUS_WT_NEW
	};

	cl_git_rewritefile("submodules/testrepo/README", "heyheyhey");
	cl_git_mkfile("submodules/testrepo/all_new.txt", "never seen before");

	/* first do a normal status, which should now include the submodule */

	memset(&counts, 0, sizeof(counts));
	counts.expected_paths = expected_files_with_sub;
	counts.expected_statuses = expected_status_with_sub;

	opts.flags = GIT_STATUS_OPT_DEFAULTS;

	cl_git_pass(
		git_status_foreach_ext(g_repo, &opts, cb_status__match, &counts));
	cl_assert_equal_i(7, counts.entry_count);

	/* try again with EXCLUDE_SUBMODULES which should skip it */

	memset(&counts, 0, sizeof(counts));
	counts.expected_paths = expected_files;
	counts.expected_statuses = expected_status;

	opts.flags = GIT_STATUS_OPT_DEFAULTS | GIT_STATUS_OPT_EXCLUDE_SUBMODULES;

	cl_git_pass(
		git_status_foreach_ext(g_repo, &opts, cb_status__match, &counts));
	cl_assert_equal_i(6, counts.entry_count);
}

