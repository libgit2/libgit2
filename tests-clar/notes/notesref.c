#include "clar_libgit2.h"

#include "notes.h"

static git_repository *_repo;

void test_notes_notesref__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
}

void test_notes_notesref__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_notes_notesref__config_corenotesref(void)
{
	const char *default_ref;

	cl_git_pass(git_note_default_ref(&default_ref, _repo));
	cl_assert(!strcmp(default_ref, GIT_NOTES_DEFAULT_REF));
}
