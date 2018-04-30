#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "submodule_helpers.h"
#include "fileops.h"
#include "repository.h"

static git_repository *g_repo = NULL;

void test_submodule_escape__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

#define EVIL_SM_NAME "../../modules/evil"

static int find_evil(git_submodule *sm, const char *name, void *payload)
{
	int *foundit = (int *) payload;

	GIT_UNUSED(sm);

	if (!git__strcmp(EVIL_SM_NAME, name))
		*foundit = true;

	return 0;
}

void test_submodule_escape__from_gitdir(void)
{
	int foundit;
	git_config *cfg;
	git_submodule *sm;
	git_buf buf = GIT_BUF_INIT;

	g_repo = setup_fixture_submodule_simple();

	cl_git_pass(git_buf_joinpath(&buf, git_repository_workdir(g_repo), ".gitmodules"));
	cl_git_pass(git_config_open_ondisk(&cfg, git_buf_cstr(&buf)));

	/* We don't have a function to rename a subsection so we do it manually */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "testrepo"));
	cl_git_pass(git_config_set_string(cfg, "submodule." EVIL_SM_NAME ".path", git_submodule_path(sm)));
	cl_git_pass(git_config_set_string(cfg, "submodule." EVIL_SM_NAME ".url", git_submodule_url(sm)));
	cl_git_pass(git_config_delete_entry(cfg, "submodule.testrepo.path"));
	cl_git_pass(git_config_delete_entry(cfg, "submodule.testrepo.url"));
	git_config_free(cfg);

	/* We also need to update the value in the config */
	cl_git_pass(git_repository_config__weakptr(&cfg, g_repo));
	cl_git_pass(git_config_set_string(cfg, "submodule." EVIL_SM_NAME ".url", git_submodule_url(sm)));
	cfg = NULL;

	/* Find it all the different ways we know about it */
	cl_git_fail_with(GIT_ENOTFOUND, git_submodule_lookup(&sm, g_repo, EVIL_SM_NAME));
	cl_git_fail_with(GIT_ENOTFOUND, git_submodule_lookup(&sm, g_repo, "testrepo"));
	foundit = 0;
	cl_git_pass(git_submodule_foreach(g_repo, find_evil, &foundit));
	cl_assert_equal_i(0, foundit);
}
