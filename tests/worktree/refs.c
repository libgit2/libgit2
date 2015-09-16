#include "clar_libgit2.h"
#include "worktree_helpers.h"

#define COMMON_REPO "testrepo"
#define WORKTREE_REPO "testrepo-worktree"

static worktree_fixture fixture =
	WORKTREE_FIXTURE_INIT(COMMON_REPO, WORKTREE_REPO);

void test_worktree_refs__initialize(void)
{
	setup_fixture_worktree(&fixture);
}

void test_worktree_refs__cleanup(void)
{
	cleanup_fixture_worktree(&fixture);
}

void test_worktree_refs__list(void)
{
	git_strarray refs, wtrefs;
	unsigned i, j;
	int error = 0;

	cl_git_pass(git_reference_list(&refs, fixture.repo));
	cl_git_pass(git_reference_list(&wtrefs, fixture.worktree));

	if (refs.count != wtrefs.count)
	{
		error = GIT_ERROR;
		goto exit;
	}

	for (i = 0; i < refs.count; i++)
	{
		int found = 0;

		for (j = 0; j < wtrefs.count; j++)
		{
			if (!strcmp(refs.strings[i], wtrefs.strings[j]))
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			error = GIT_ERROR;
			goto exit;
		}
	}

exit:
	git_strarray_free(&refs);
	git_strarray_free(&wtrefs);
	cl_git_pass(error);
}

void test_worktree_refs__read_head(void)
{
	git_reference *head;

	cl_git_pass(git_repository_head(&head, fixture.worktree));

	git_reference_free(head);
}
