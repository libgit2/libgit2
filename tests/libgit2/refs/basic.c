#include "clar_libgit2.h"

#include "futils.h"
#include "refs.h"
#include "ref_helpers.h"

static git_repository *g_repo;

static const char *loose_tag_ref_name = "refs/tags/e90810b";

void test_refs_basic__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo");
	cl_git_pass(git_repository_set_ident(g_repo, "me", "foo@example.com"));
}

void test_refs_basic__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refs_basic__reference_realloc(void)
{
	git_reference *ref;
	git_reference *new_ref;
	const char *new_name = "refs/tags/awful/name-which-is/clearly/really-that-much/longer-than/the-old-one";

	/* Retrieval of the reference to rename */
	cl_git_pass(git_reference_lookup(&ref, g_repo, loose_tag_ref_name));

	new_ref = git_reference__realloc(&ref, new_name);
	cl_assert(new_ref != NULL);
	git_reference_free(new_ref);
	git_reference_free(ref);

	/* Reload, so we restore the value */
	cl_git_pass(git_reference_lookup(&ref, g_repo, loose_tag_ref_name));

	cl_git_pass(git_reference_rename(&new_ref, ref, new_name, 1, "log message"));
	cl_assert(ref != NULL);
	cl_assert(new_ref != NULL);
	git_reference_free(new_ref);
	git_reference_free(ref);
}
