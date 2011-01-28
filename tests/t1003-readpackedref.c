#include "test_lib.h"
#include "test_helpers.h"
#include "refs.h"

static const char *packed_head_name = "refs/heads/packed";
static const char *packed_test_head_name = "refs/heads/packed-test";

BEGIN_TEST(packed_reference_looking_up)
	git_repository *repo;
	git_reference *reference;
	git_object *object;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, packed_head_name));
	must_be_true(reference->type == GIT_REF_OID);
	must_be_true(reference->packed == 1);
	must_be_true(strcmp(reference->name, packed_head_name) == 0);

	must_pass(git_repository_lookup(&object, repo, git_reference_oid(reference), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_repository_free(repo);
END_TEST

BEGIN_TEST(packed_exists_but_more_recent_loose_reference_is_retrieved)
	git_repository *repo;
	git_reference *reference;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_lookup_ref(&reference, repo, packed_head_name));
	must_pass(git_repository_lookup_ref(&reference, repo, packed_test_head_name));
	must_be_true(reference->type == GIT_REF_OID);
	must_be_true(reference->packed == 0);
	must_be_true(strcmp(reference->name, packed_test_head_name) == 0);

	git_repository_free(repo);
END_TEST
