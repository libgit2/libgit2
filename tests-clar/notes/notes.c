#include "clar_libgit2.h"

static git_repository *_repo;

void test_notes_notes__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
}

void test_notes_notes__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_notes_notes__1(void)
{
	git_oid oid;
	static git_note *note;

	cl_git_pass(git_oid_fromstr(&oid, "a8233120f6ad708f843d861ce2b7228ec4e3dec6"));

	cl_git_pass(git_note_read(&note, _repo, NULL, &oid));
	cl_git_pass(git_note_read(&note, _repo, "refs/notes/commits", &oid));

	cl_assert_equal_s(git_note_message(note), "Hello Git Notes\n");

	git_note_free(note);
}

static struct {
	const char *note_sha;
	const char *annotated_object_sha;
} list_expectations[] = {
	{ "774ac7f437b7f86245b3ba4cc649b0a74859f4fc", "9fd738e8f7967c078dceed8190330fc8648ee56a" },
	{ "b2d158a0df675c54a7e63a32b412df8fb1023af3", "a8233120f6ad708f843d861ce2b7228ec4e3dec6" },
	{ "ab6c1c6dc1dc6c9a4a59eb478b45a40bc9c4be8c", "c47800c7266a2be04c571c04d5a6614691ea99bd" },
	{ NULL, NULL }
};

#define EXPECTATIONS_COUNT (sizeof(list_expectations)/sizeof(list_expectations[0])) - 1

static int note_list_cb(git_note_data *note_data, void *payload)
{
	git_oid expected_note_oid, expected_target_oid;

	unsigned int *count = (unsigned int *)payload;

	cl_assert(*count < EXPECTATIONS_COUNT);

	cl_git_pass(git_oid_fromstr(&expected_note_oid, list_expectations[*count].note_sha));
	cl_assert(git_oid_cmp(&expected_note_oid, &note_data->blob_oid) == 0);

	cl_git_pass(git_oid_fromstr(&expected_target_oid, list_expectations[*count].annotated_object_sha));
	cl_assert(git_oid_cmp(&expected_target_oid, &note_data->annotated_object_oid) == 0);

	(*count)++;

	return 0;
}

void test_notes_notes__can_retrieve_a_list_of_notes_for_a_given_namespace(void)
{
	unsigned int retrieved_notes = 0;

	cl_git_pass(git_note_foreach(_repo, NULL, note_list_cb, &retrieved_notes));

	cl_assert_equal_i(3, retrieved_notes);
}

void test_notes_notes__retrieving_a_list_of_notes_for_an_unknown_namespace_returns_ENOTFOUND(void)
{
	int error;
	unsigned int retrieved_notes = 0;

	error = git_note_foreach(_repo, "refs/notes/i-am-not", note_list_cb, &retrieved_notes);
	cl_git_fail(error);
	cl_assert_equal_i(GIT_ENOTFOUND, error);

	cl_assert_equal_i(0, retrieved_notes);
}
