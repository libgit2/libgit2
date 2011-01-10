#include "test_lib.h"
#include "test_helpers.h"
#include "refs.h"

static const char *head_ref_name = "HEAD";
static const char *current_head_target = "refs/heads/master";
static const char *current_master_tip = "be3563ae3f795b2b4353bcce3a527ad0a4f7f644";

BEGIN_TEST(symbolic_reference_looking_up)
	git_repository *repo;
	git_reference *reference;
	git_reference_symbolic *head_ref;
	git_reference_object_id *target_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));


	must_pass(git_repository_reference_lookup(&reference, repo, head_ref_name));
	must_be_true(reference->type == GIT_REF_SYMBOLIC);
	must_be_true(reference->is_packed == 0);
	must_be_true(strcmp(reference->name, head_ref_name) == 0);

	head_ref = (git_reference_symbolic *)reference;
	must_be_true(head_ref->target != NULL);
	must_be_true(head_ref->target->type == GIT_REF_OBJECT_ID);					/* Current HEAD directly points to the object id reference */
	must_be_true(strcmp(head_ref->target->name, current_head_target) == 0);

	target_ref = (git_reference_object_id *)head_ref->target;

	must_pass(git_repository_lookup(&object, repo, &target_ref->id, GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_mkstr(&id, current_master_tip);
	must_be_true(git_oid_cmp(&id, git_object_id(object)) == 0);


	git_repository_free(repo);
END_TEST

BEGIN_TEST(looking_up_head_then_master)
	git_repository *repo;
	git_reference *reference;
	git_reference_symbolic *head_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));


	must_pass(git_repository_reference_lookup(&reference, repo, head_ref_name));

	head_ref = (git_reference_symbolic *)reference;
	must_be_true(head_ref->target != NULL);

	must_pass(git_repository_reference_lookup(&reference, repo, current_head_target));
	must_be_true(head_ref->target == reference);


	git_repository_free(repo);
END_TEST

BEGIN_TEST(looking_up_master_then_head)
	git_repository *repo;
	git_reference *reference, *master_ref;
	git_reference_symbolic *head_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));


	must_pass(git_repository_reference_lookup(&master_ref, repo, current_head_target));

	must_pass(git_repository_reference_lookup(&reference, repo, head_ref_name));
	head_ref = (git_reference_symbolic *)reference;
	must_be_true(head_ref->target != NULL);
	
	must_be_true(head_ref->target == master_ref);


	git_repository_free(repo);
END_TEST