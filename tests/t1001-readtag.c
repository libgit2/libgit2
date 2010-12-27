#include "test_lib.h"
#include "test_helpers.h"
#include "refs.h"

static const char *loose_tag_ref_name = "refs/tags/test";
static const char *non_existing_tag_ref_name = "refs/tags/i-do-not-exist";

BEGIN_TEST(loose_tag_reference_looking_up)
	git_repository *repo;
	git_reference *reference;
	git_object *object;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));


	must_pass(git_repository_reference_lookup(&reference, repo, loose_tag_ref_name));
	must_be_true(reference->type == GIT_REF_OBJECT_ID);
	must_be_true(reference->is_packed == 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);


	must_pass(git_repository_lookup(&object, repo, &((git_reference_object_id *)reference)->id, GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_TAG);


	git_repository_free(repo);
END_TEST

BEGIN_TEST(non_existing_tag_reference_looking_up)
	git_repository *repo;
	git_reference *reference;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_fail(git_repository_reference_lookup(&reference, repo, non_existing_tag_ref_name));

	git_repository_free(repo);
END_TEST