#include "clar_libgit2.h"
#include "refs.h"
#include "repo_helpers.h"

void make_head_orphaned(git_repository* repo, const char *target)
{
	git_reference *head;

	cl_git_pass(git_reference_create_symbolic(&head, repo, GIT_HEAD_FILE, target, 1));
	git_reference_free(head);
}
