#include "clar_libgit2.h"
#include "array.h"

void test_core_cancellable__can_cancel(void)
{
	git_cancellable_source *cs;
	git_cancellable *token;

	cl_git_pass(git_cancellable_source_new(&cs));
	token = git_cancellable_source_token(cs);

	cl_assert(!git_cancellable_is_cancelled(token));
	git_cancellable_source_cancel(cs);
	cl_assert(git_cancellable_is_cancelled(token));

	git_cancellable_source_free(cs);
}

void cancel_second(git_cancellable *cancellable, void *payload)
{
	git_cancellable_source *cs;

	GIT_UNUSED(cancellable);

	cs = (git_cancellable_source *) payload;
	git_cancellable_source_cancel(cs);
}

void test_core_cancellable__can_register(void)
{
	git_cancellable_source *cs1, *cs2;
	git_cancellable *token1, *token2;

	cl_git_pass(git_cancellable_source_new(&cs1));
	token1 = git_cancellable_source_token(cs1);

	cl_git_pass(git_cancellable_source_new(&cs2));
	token2 = git_cancellable_source_token(cs2);

	cl_git_pass(git_cancellable_register(token1, cancel_second, cs2));
	cl_assert(!git_cancellable_is_cancelled(token1));
	cl_assert(!git_cancellable_is_cancelled(token2));
	git_cancellable_source_cancel(cs1);
	cl_assert(git_cancellable_is_cancelled(token1));
	cl_assert(git_cancellable_is_cancelled(token2));

	git_cancellable_source_free(cs1);
	git_cancellable_source_free(cs2);
}
