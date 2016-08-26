#include "clar_libgit2.h"

#include "repository.h"
#include "git2/reflog.h"
#include "reflog.h"

static const char *loose_tag_ref_name = "refs/tags/e90810b";
static const char *non_existing_tag_ref_name = "refs/tags/i-do-not-exist";
static const char *head_tracker_sym_ref_name = "head-tracker";
static const char *current_head_target = "refs/heads/master";
static const char *current_master_tip = "a65fedf39aefe402d3bb6e24df4d4f5fe4547750";
static const char *packed_head_name = "refs/heads/packed";
static const char *packed_test_head_name = "refs/heads/packed-test";

static git_repository *g_repo;

void test_refs_read__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_refs_read__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

void test_refs_read__loose_tag(void)
{
   // lookup a loose tag reference
	git_reference *reference;
	git_object *object;
	git_buf ref_name_from_tag_name = GIT_BUF_INIT;

	cl_git_pass(git_reference_lookup(&reference, g_repo, loose_tag_ref_name));
	cl_assert(git_reference_type(reference) & GIT_REF_OID);
	cl_assert(git_reference_is_packed(reference) == 0);
	cl_assert_equal_s(reference->name, loose_tag_ref_name);

	cl_git_pass(git_object_lookup(&object, g_repo, git_reference_oid(reference), GIT_OBJ_ANY));
	cl_assert(object != NULL);
	cl_assert(git_object_type(object) == GIT_OBJ_TAG);

	/* Ensure the name of the tag matches the name of the reference */
	cl_git_pass(git_buf_joinpath(&ref_name_from_tag_name, GIT_REFS_TAGS_DIR, git_tag_name((git_tag *)object)));
	cl_assert_equal_s(ref_name_from_tag_name.ptr, loose_tag_ref_name);
	git_buf_free(&ref_name_from_tag_name);

	git_object_free(object);

	git_reference_free(reference);
}

void test_refs_read__nonexisting_tag(void)
{
   // lookup a loose tag reference that doesn't exist
	git_reference *reference;

	cl_git_fail(git_reference_lookup(&reference, g_repo, non_existing_tag_ref_name));

	git_reference_free(reference);
}


void test_refs_read__symbolic(void)
{
   // lookup a symbolic reference
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	cl_git_pass(git_reference_lookup(&reference, g_repo, GIT_HEAD_FILE));
	cl_assert(git_reference_type(reference) & GIT_REF_SYMBOLIC);
	cl_assert(git_reference_is_packed(reference) == 0);
	cl_assert_equal_s(reference->name, GIT_HEAD_FILE);

	cl_git_pass(git_reference_resolve(&resolved_ref, reference));
	cl_assert(git_reference_type(resolved_ref) == GIT_REF_OID);

	cl_git_pass(git_object_lookup(&object, g_repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	cl_assert(object != NULL);
	cl_assert(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_fromstr(&id, current_master_tip);
	cl_assert(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_object_free(object);

	git_reference_free(reference);
	git_reference_free(resolved_ref);
}

void test_refs_read__nested_symbolic(void)
{
   // lookup a nested symbolic reference
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	cl_git_pass(git_reference_lookup(&reference, g_repo, head_tracker_sym_ref_name));
	cl_assert(git_reference_type(reference) & GIT_REF_SYMBOLIC);
	cl_assert(git_reference_is_packed(reference) == 0);
	cl_assert_equal_s(reference->name, head_tracker_sym_ref_name);

	cl_git_pass(git_reference_resolve(&resolved_ref, reference));
	cl_assert(git_reference_type(resolved_ref) == GIT_REF_OID);

	cl_git_pass(git_object_lookup(&object, g_repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	cl_assert(object != NULL);
	cl_assert(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_fromstr(&id, current_master_tip);
	cl_assert(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_object_free(object);

	git_reference_free(reference);
	git_reference_free(resolved_ref);
}

void test_refs_read__head_then_master(void)
{
   // lookup the HEAD and resolve the master branch
	git_reference *reference, *resolved_ref, *comp_base_ref;

	cl_git_pass(git_reference_lookup(&reference, g_repo, head_tracker_sym_ref_name));
	cl_git_pass(git_reference_resolve(&comp_base_ref, reference));
	git_reference_free(reference);

	cl_git_pass(git_reference_lookup(&reference, g_repo, GIT_HEAD_FILE));
	cl_git_pass(git_reference_resolve(&resolved_ref, reference));
	cl_git_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));
	git_reference_free(reference);
	git_reference_free(resolved_ref);

	cl_git_pass(git_reference_lookup(&reference, g_repo, current_head_target));
	cl_git_pass(git_reference_resolve(&resolved_ref, reference));
	cl_git_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));
	git_reference_free(reference);
	git_reference_free(resolved_ref);

	git_reference_free(comp_base_ref);
}

void test_refs_read__master_then_head(void)
{
   // lookup the master branch and then the HEAD
	git_reference *reference, *master_ref, *resolved_ref;

	cl_git_pass(git_reference_lookup(&master_ref, g_repo, current_head_target));
	cl_git_pass(git_reference_lookup(&reference, g_repo, GIT_HEAD_FILE));

	cl_git_pass(git_reference_resolve(&resolved_ref, reference));
	cl_git_pass(git_oid_cmp(git_reference_oid(master_ref), git_reference_oid(resolved_ref)));

	git_reference_free(reference);
	git_reference_free(resolved_ref);
	git_reference_free(master_ref);
}


void test_refs_read__packed(void)
{
   // lookup a packed reference
	git_reference *reference;
	git_object *object;

	cl_git_pass(git_reference_lookup(&reference, g_repo, packed_head_name));
	cl_assert(git_reference_type(reference) & GIT_REF_OID);
	cl_assert(git_reference_is_packed(reference));
	cl_assert_equal_s(reference->name, packed_head_name);

	cl_git_pass(git_object_lookup(&object, g_repo, git_reference_oid(reference), GIT_OBJ_ANY));
	cl_assert(object != NULL);
	cl_assert(git_object_type(object) == GIT_OBJ_COMMIT);

	git_object_free(object);

	git_reference_free(reference);
}

void test_refs_read__loose_first(void)
{
   // assure that a loose reference is looked up before a packed reference
	git_reference *reference;

	cl_git_pass(git_reference_lookup(&reference, g_repo, packed_head_name));
	git_reference_free(reference);
	cl_git_pass(git_reference_lookup(&reference, g_repo, packed_test_head_name));
	cl_assert(git_reference_type(reference) & GIT_REF_OID);
	cl_assert(git_reference_is_packed(reference) == 0);
	cl_assert_equal_s(reference->name, packed_test_head_name);

	git_reference_free(reference);
}
