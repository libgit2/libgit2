#include "clar_libgit2.h"

static git_repository *_repo;
static git_note *_note;
static git_blob *_blob;
static git_signature *_sig;

void test_notes_notes__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
	cl_git_pass(git_repository_open(&_repo, "testrepo.git"));
}

void test_notes_notes__cleanup(void)
{
	git_note_free(_note);
	git_blob_free(_blob);
	git_signature_free(_sig);

	git_repository_free(_repo);
	cl_fixture_cleanup("testrepo.git");
}

void test_notes_notes__1(void)
{
	git_oid oid, note_oid;

	cl_git_pass(git_signature_now(&_sig, "alice", "alice@example.com"));
	cl_git_pass(git_oid_fromstr(&oid, "8496071c1b46c854b31185ea97743be6a8774479"));

	cl_git_pass(git_note_create(&note_oid, _repo, _sig, _sig, "refs/notes/some/namespace", &oid, "hello world\n"));
	cl_git_pass(git_note_create(&note_oid, _repo, _sig, _sig, NULL, &oid, "hello world\n"));

	cl_git_pass(git_note_read(&_note, _repo, NULL, &oid));

	cl_assert_strequal(git_note_message(_note), "hello world\n");
	cl_assert(!git_oid_cmp(git_note_oid(_note), &note_oid));

	cl_git_pass(git_blob_lookup(&_blob, _repo, &note_oid));
	cl_assert_strequal(git_note_message(_note), git_blob_rawcontent(_blob));

	cl_git_fail(git_note_create(&note_oid, _repo, _sig, _sig, NULL, &oid, "hello world\n"));
	cl_git_fail(git_note_create(&note_oid, _repo, _sig, _sig, "refs/notes/some/namespace", &oid, "hello world\n"));

	cl_git_pass(git_note_remove(_repo, NULL, _sig, _sig, &oid));
	cl_git_pass(git_note_remove(_repo, "refs/notes/some/namespace", _sig, _sig, &oid));

	cl_git_fail(git_note_remove(_repo, NULL, _sig, _sig, &note_oid));
	cl_git_fail(git_note_remove(_repo, "refs/notes/some/namespace", _sig, _sig, &oid));
}
