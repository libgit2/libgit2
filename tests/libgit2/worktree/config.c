#include "clar_libgit2.h"
#include "worktree_helpers.h"

#define COMMON_REPO "testrepo"
#define WORKTREE_REPO "testrepo-worktree"

static worktree_fixture fixture =
	WORKTREE_FIXTURE_INIT(COMMON_REPO, WORKTREE_REPO);
static worktree_fixture submodule =
	WORKTREE_FIXTURE_INIT("submodules", "submodules-worktree-parent");

void test_worktree_config__initialize(void)
{
	setup_fixture_worktree(&fixture);
	setup_fixture_worktree(&submodule);
}

void test_worktree_config__cleanup(void)
{
	cleanup_fixture_worktree(&fixture);
	cleanup_fixture_worktree(&submodule);
}

void test_worktree_config__open(void)
{
	git_config *cfg;

	cl_git_pass(git_repository_config(&cfg, fixture.worktree));
	cl_assert(cfg != NULL);

	git_config_free(cfg);
}

void test_worktree_config__set_level_local(void)
{
	git_config *cfg;
	int32_t val;

	cl_git_pass(git_repository_config(&cfg, fixture.worktree));
	cl_git_pass(git_config_set_int32(cfg, "core.dummy", 5));
	git_config_free(cfg);

	/*
	 * reopen to verify configuration has been set in the
	 * common dir
	 */
	cl_git_pass(git_repository_config(&cfg, fixture.repo));
	cl_git_pass(git_config_get_int32(&val, cfg, "core.dummy"));
	cl_assert_equal_i(val, 5);
	git_config_free(cfg);
}

void test_worktree_config__requires_extension(void)
{
	git_config *cfg;
	git_config *wtcfg;
	int extension = 0;

	/*
	 * the "submodules" repo does not have extensions.worktreeconfig
	 * set, the worktree configuration should not be available.
	 */
	cl_git_pass(git_repository_config(&cfg, submodule.repo));
	cl_git_fail_with(GIT_ENOTFOUND, git_config_get_bool(&extension, cfg, "extensions.worktreeconfig"));
	cl_assert_equal_i(0, extension);
	cl_git_fail_with(GIT_ENOTFOUND, git_config_open_level(&wtcfg, cfg, GIT_CONFIG_LEVEL_WORKTREE));
	git_config_free(cfg);

	/* the "testrepo" repo does have the configuration set. */
	cl_git_pass(git_repository_config(&cfg, fixture.repo));
	cl_git_pass(git_config_get_bool(&extension, cfg, "extensions.worktreeconfig"));
	cl_assert_equal_i(1, extension);
	cl_git_pass(git_config_open_level(&wtcfg, cfg, GIT_CONFIG_LEVEL_WORKTREE));
	git_config_free(wtcfg);
	git_config_free(cfg);
}

void test_worktree_config__exists(void)
{
	git_config *cfg, *wtcfg, *snap;
	const char *str;

	cl_git_pass(git_repository_config(&cfg, fixture.repo));
	cl_git_pass(git_repository_config(&wtcfg, fixture.worktree));

	cl_git_pass(git_config_snapshot(&snap, cfg));
	cl_git_pass(git_config_get_string(&str, snap, "worktreetest.config"));
	cl_assert_equal_s("mainrepo", str);
	git_config_free(snap);

	cl_git_pass(git_config_snapshot(&snap, wtcfg));
	cl_git_pass(git_config_get_string(&str, snap, "worktreetest.config"));
	cl_assert_equal_s("worktreerepo", str);
	git_config_free(snap);

	git_config_free(cfg);
	git_config_free(wtcfg);
}

void test_worktree_config__set_level_worktree(void)
{
	git_config *cfg;
	git_config *wtcfg;
	int32_t val;

	cl_git_pass(git_repository_config(&cfg, fixture.repo));
	cl_git_pass(git_config_open_level(&wtcfg, cfg, GIT_CONFIG_LEVEL_WORKTREE));
	cl_git_pass(git_config_set_int32(wtcfg, "worktree.specific", 42));

	cl_git_pass(git_config_get_int32(&val, cfg, "worktree.specific"));
	cl_assert_equal_i(val, 42);

	/* reopen to verify config has been set */
	git_config_free(cfg);
	cl_git_pass(git_repository_config(&cfg, fixture.repo));
	cl_git_pass(git_config_get_int32(&val, cfg, "worktree.specific"));
	cl_assert_equal_i(val, 42);

	cl_git_fail_with(GIT_ENOTFOUND, git_config_delete_entry(cfg, "worktree.specific"));

	cl_git_pass(git_config_delete_entry(wtcfg, "worktree.specific"));
	cl_git_fail_with(GIT_ENOTFOUND, git_config_get_int32(&val, cfg, "worktree.specific"));

	git_config_free(cfg);
	git_config_free(wtcfg);
}
