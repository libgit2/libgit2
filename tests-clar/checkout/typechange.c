#include "clar_libgit2.h"
#include "git2/checkout.h"
#include "path.h"
#include "posix.h"

static git_repository *g_repo = NULL;

static const char *g_typechange_oids[] = {
	"79b9f23e85f55ea36a472a902e875bc1121a94cb",
	"9bdb75b73836a99e3dbeea640a81de81031fdc29",
	"0e7ed140b514b8cae23254cb8656fe1674403aff",
	"9d0235c7a7edc0889a18f97a42ee6db9fe688447",
	"9b19edf33a03a0c59cdfc113bfa5c06179bf9b1a",
	"1b63caae4a5ca96f78e8dfefc376c6a39a142475",
	"6eae26c90e8ccc4d16208972119c40635489c6f0",
	NULL
};

static bool g_typechange_empty[] = {
	true, false, false, false, false, false, true, true
};

void test_checkout_typechange__initialize(void)
{
	g_repo = cl_git_sandbox_init("typechanges");

	cl_fixture_sandbox("submod2_target");
	p_rename("submod2_target/.gitted", "submod2_target/.git");
}

void test_checkout_typechange__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_fixture_cleanup("submod2_target");
}

void test_checkout_typechange__checkout_typechanges(void)
{
	int i;
	git_object *obj;
	git_checkout_opts opts = {0};

	opts.checkout_strategy =
		GIT_CHECKOUT_REMOVE_UNTRACKED |
		GIT_CHECKOUT_CREATE_MISSING |
		GIT_CHECKOUT_OVERWRITE_MODIFIED;

	for (i = 0; g_typechange_oids[i] != NULL; ++i) {
		cl_git_pass(git_revparse_single(&obj, g_repo, g_typechange_oids[i]));
		/* fprintf(stderr, "checking out '%s'\n", g_typechange_oids[i]); */

		cl_git_pass(git_checkout_tree(g_repo, obj, &opts, NULL));

		git_object_free(obj);

		if (!g_typechange_empty[i]) {
			cl_assert(git_path_isdir("typechanges"));
			cl_assert(git_path_exists("typechanges/a"));
			cl_assert(git_path_exists("typechanges/b"));
			cl_assert(git_path_exists("typechanges/c"));
			cl_assert(git_path_exists("typechanges/d"));
			cl_assert(git_path_exists("typechanges/e"));
		} else {
			cl_assert(git_path_isdir("typechanges"));
			cl_assert(!git_path_exists("typechanges/a"));
			cl_assert(!git_path_exists("typechanges/b"));
			cl_assert(!git_path_exists("typechanges/c"));
			cl_assert(!git_path_exists("typechanges/d"));
			cl_assert(!git_path_exists("typechanges/e"));
		}
	}
}
