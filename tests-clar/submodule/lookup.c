#include "clar_libgit2.h"
#include "submodule_helpers.h"
#include "posix.h"

static git_repository *g_repo = NULL;

void test_submodule_lookup__initialize(void)
{
	g_repo = cl_git_sandbox_init("submod2");

	cl_fixture_sandbox("submod2_target");
	p_rename("submod2_target/.gitted", "submod2_target/.git");

	/* must create submod2_target before rewrite so prettify will work */
	rewrite_gitmodules(git_repository_workdir(g_repo));
	p_rename("submod2/not_submodule/.gitted", "submod2/not_submodule/.git");
}

void test_submodule_lookup__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("submod2_target");
}

void test_submodule_lookup__simple_lookup(void)
{
	git_submodule *sm;

	/* lookup existing */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_assert(sm);

	/* lookup pending change in .gitmodules that is not in HEAD */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_assert(sm);

	/* lookup pending change in .gitmodules that is neither in HEAD nor index */
	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_gitmodules_only"));
	cl_assert(sm);

	/* lookup git repo subdir that is not added as submodule */
	cl_assert(git_submodule_lookup(&sm, g_repo, "not_submodule") == GIT_EEXISTS);

	/* lookup existing directory that is not a submodule */
	cl_assert(git_submodule_lookup(&sm, g_repo, "just_a_dir") == GIT_ENOTFOUND);

	/* lookup existing file that is not a submodule */
	cl_assert(git_submodule_lookup(&sm, g_repo, "just_a_file") == GIT_ENOTFOUND);

	/* lookup non-existent item */
	cl_assert(git_submodule_lookup(&sm, g_repo, "no_such_file") == GIT_ENOTFOUND);
}

void test_submodule_lookup__accessors(void)
{
	git_submodule *sm;
	const char *oid = "480095882d281ed676fe5b863569520e54a7d5c0";

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_unchanged"));
	cl_assert(git_submodule_owner(sm) == g_repo);
	cl_assert_equal_s("sm_unchanged", git_submodule_name(sm));
	cl_assert(git__suffixcmp(git_submodule_path(sm), "sm_unchanged") == 0);
	cl_assert(git__suffixcmp(git_submodule_url(sm), "/submod2_target") == 0);

	cl_assert(git_oid_streq(git_submodule_index_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_head_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_wd_id(sm), oid) == 0);

	cl_assert(git_submodule_ignore(sm) == GIT_SUBMODULE_IGNORE_NONE);
	cl_assert(git_submodule_update(sm) == GIT_SUBMODULE_UPDATE_CHECKOUT);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_changed_head"));
	cl_assert_equal_s("sm_changed_head", git_submodule_name(sm));

	cl_assert(git_oid_streq(git_submodule_index_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_head_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_wd_id(sm),
		"3d9386c507f6b093471a3e324085657a3c2b4247") == 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_added_and_uncommited"));
	cl_assert_equal_s("sm_added_and_uncommited", git_submodule_name(sm));

	cl_assert(git_oid_streq(git_submodule_index_id(sm), oid) == 0);
	cl_assert(git_submodule_head_id(sm) == NULL);
	cl_assert(git_oid_streq(git_submodule_wd_id(sm), oid) == 0);

	cl_git_pass(git_submodule_lookup(&sm, g_repo, "sm_missing_commits"));
	cl_assert_equal_s("sm_missing_commits", git_submodule_name(sm));

	cl_assert(git_oid_streq(git_submodule_index_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_head_id(sm), oid) == 0);
	cl_assert(git_oid_streq(git_submodule_wd_id(sm),
		"5e4963595a9774b90524d35a807169049de8ccad") == 0);
}

typedef struct {
	int count;
} sm_lookup_data;

static int sm_lookup_cb(git_submodule *sm, const char *name, void *payload)
{
	sm_lookup_data *data = payload;
	data->count += 1;
	cl_assert_equal_s(git_submodule_name(sm), name);
	return 0;
}

void test_submodule_lookup__foreach(void)
{
	sm_lookup_data data;
	memset(&data, 0, sizeof(data));
	cl_git_pass(git_submodule_foreach(g_repo, sm_lookup_cb, &data));
	cl_assert_equal_i(8, data.count);
}
