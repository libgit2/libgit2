/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include "repository.h"

static const char *loose_tag_ref_name = "refs/tags/test";
static const char *non_existing_tag_ref_name = "refs/tags/i-do-not-exist";

BEGIN_TEST("readtag", loose_tag_reference_looking_up)
	git_repository *repo;
	git_reference *reference;
	git_object *object;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, loose_tag_ref_name));
	must_be_true(reference->type == GIT_REF_OID);
	must_be_true(reference->packed == 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);

	must_pass(git_repository_lookup(&object, repo, git_reference_oid(reference), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_TAG);

	git_repository_free(repo);
END_TEST

BEGIN_TEST("readtag", non_existing_tag_reference_looking_up)
	git_repository *repo;
	git_reference *reference;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_fail(git_repository_lookup_ref(&reference, repo, non_existing_tag_ref_name));

	git_repository_free(repo);
END_TEST

static const char *head_ref_name = "HEAD";
static const char *head_tracker_sym_ref_name = "head-tracker";
static const char *current_head_target = "refs/heads/master";
static const char *current_master_tip = "be3563ae3f795b2b4353bcce3a527ad0a4f7f644";

BEGIN_TEST("readsymref", symbolic_reference_looking_up)
	git_repository *repo;
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, head_ref_name));
	must_be_true(reference->type == GIT_REF_SYMBOLIC);
	must_be_true(reference->packed == 0);
	must_be_true(strcmp(reference->name, head_ref_name) == 0);

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	must_pass(git_repository_lookup(&object, repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_mkstr(&id, current_master_tip);
	must_be_true(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_repository_free(repo);
END_TEST

BEGIN_TEST("readsymref", nested_symbolic_reference_looking_up)
	git_repository *repo;
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, head_tracker_sym_ref_name));
	must_be_true(reference->type == GIT_REF_SYMBOLIC);
	must_be_true(reference->packed == 0);
	must_be_true(strcmp(reference->name, head_tracker_sym_ref_name) == 0);

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	must_pass(git_repository_lookup(&object, repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_mkstr(&id, current_master_tip);
	must_be_true(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_repository_free(repo);
END_TEST

BEGIN_TEST("readsymref", looking_up_head_then_master)
	git_repository *repo;
	git_reference *reference, *resolved_ref, *comp_base_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, head_tracker_sym_ref_name));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	comp_base_ref = resolved_ref;

	must_pass(git_repository_lookup_ref(&reference, repo, head_ref_name));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));

	must_pass(git_repository_lookup_ref(&reference, repo, current_head_target));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));

	git_repository_free(repo);
END_TEST

BEGIN_TEST("readsymref", looking_up_master_then_head)
	git_repository *repo;
	git_reference *reference, *master_ref, *resolved_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&master_ref, repo, current_head_target));
	must_pass(git_repository_lookup_ref(&reference, repo, head_ref_name));

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(master_ref), git_reference_oid(resolved_ref)));

	git_repository_free(repo);
END_TEST

static const char *packed_head_name = "refs/heads/packed";
static const char *packed_test_head_name = "refs/heads/packed-test";

BEGIN_TEST("readpackedref", packed_reference_looking_up)
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

BEGIN_TEST("readpackedref", packed_exists_but_more_recent_loose_reference_is_retrieved)
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

BEGIN_TEST("createref", create_new_symbolic_ref)
	git_reference *new_reference, *looked_up_ref, *resolved_ref;
	git_repository *repo;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head_tracker = "another-head-tracker";

	git_oid_mkstr(&id, current_master_tip);

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* Retrieve the physical path to the symbolic ref for further cleaning */
	git__joinpath(ref_path, repo->path_repository, new_head_tracker);

	/* Create and write the new symbolic reference */
	must_pass(git_reference_new(&new_reference, repo));
	git_reference_set_target(new_reference, current_head_target);
	git_reference_set_name(new_reference, new_head_tracker);
	must_pass(git_reference_write(new_reference));

	/* Ensure the reference can be looked-up... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head_tracker));
	must_be_true(looked_up_ref->type == GIT_REF_SYMBOLIC);
	must_be_true(looked_up_ref->packed == 0);
	must_be_true(strcmp(looked_up_ref->name, new_head_tracker) == 0);

	/* ...peeled.. */
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	/* ...and that it points to the current master tip */
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	git_repository_free(repo);

	/* Similar test with a fresh new repository */
	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head_tracker));
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	git_repository_free(repo);

	must_pass(gitfo_unlink(ref_path));	/* TODO: replace with git_reference_delete() when available */
END_TEST

BEGIN_TEST("createref", create_new_object_id_ref)
	git_reference *new_reference, *looked_up_ref;
	git_repository *repo;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head = "refs/heads/new-head";

	git_oid_mkstr(&id, current_master_tip);

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* Retrieve the physical path to the symbolic ref for further cleaning */
	git__joinpath(ref_path, repo->path_repository, new_head);

	/* Create and write the new object id reference */
	must_pass(git_reference_new(&new_reference, repo));
	git_reference_set_oid(new_reference, &id);
	git_reference_set_name(new_reference, new_head);
	must_pass(git_reference_write(new_reference));

	/* Ensure the reference can be looked-up... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head));
	must_be_true(looked_up_ref->type == GIT_REF_OID);
	must_be_true(looked_up_ref->packed == 0);
	must_be_true(strcmp(looked_up_ref->name, new_head) == 0);

	/* ...and that it points to the current master tip */
	must_be_true(git_oid_cmp(&id, git_reference_oid(looked_up_ref)) == 0);

	git_repository_free(repo);

	/* Similar test with a fresh new repository */
	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head));
	must_be_true(git_oid_cmp(&id, git_reference_oid(looked_up_ref)) == 0);

	git_repository_free(repo);

	must_pass(gitfo_unlink(ref_path));	/* TODO: replace with git_reference_delete() when available */
END_TEST

static int ensure_refname_normalized(git_rtype ref_type, const char *input_refname, const char *expected_refname)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_PATH_MAX];

	error = git_reference__normalize_name(buffer_out, input_refname, ref_type);
	if (error < GIT_SUCCESS)
		return error;

	if (expected_refname == NULL)
		return error;

	if (strcmp(buffer_out, expected_refname))
		error = GIT_ERROR;

	return error;
}

BEGIN_TEST("normalizeref", normalize_unknown_ref_type)
	must_fail(ensure_refname_normalized(GIT_REF_INVALID, "a", NULL));
END_TEST

BEGIN_TEST("normalizeref", normalize_object_id_ref)
	must_fail(ensure_refname_normalized(GIT_REF_OID, "a", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/a/", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/a.", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/a.lock", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/dummy/a", NULL));
	must_pass(ensure_refname_normalized(GIT_REF_OID, "refs/tags/a", "refs/tags/a"));
	must_pass(ensure_refname_normalized(GIT_REF_OID, "refs/heads/a/b", "refs/heads/a/b"));
	must_pass(ensure_refname_normalized(GIT_REF_OID, "refs/heads/a./b", "refs/heads/a./b"));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/foo?bar", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads\foo", NULL));
	must_pass(ensure_refname_normalized(GIT_REF_OID, "refs/heads/v@ation", "refs/heads/v@ation"));
	must_pass(ensure_refname_normalized(GIT_REF_OID, "refs///heads///a", "refs/heads/a"));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/.a/b", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/foo/../bar", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/foo..bar", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/./foo", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_OID, "refs/heads/v@{ation", NULL));
END_TEST

BEGIN_TEST("normalizeref", normalize_symbolic_ref)
	must_pass(ensure_refname_normalized(GIT_REF_SYMBOLIC, "a", "a"));
	must_fail(ensure_refname_normalized(GIT_REF_SYMBOLIC, "", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_SYMBOLIC, "a/b", NULL));
	must_fail(ensure_refname_normalized(GIT_REF_SYMBOLIC, "heads\foo", NULL));
END_TEST


BEGIN_TEST("normalizeref", normalize_any_ref) /* Slash related rules do not apply, neither do 'refs' prefix related rules */
	must_pass(ensure_refname_normalized(GIT_REF_ANY, "a", "a"));
	must_pass(ensure_refname_normalized(GIT_REF_ANY, "a/b", "a/b"));
	must_pass(ensure_refname_normalized(GIT_REF_ANY, "refs///heads///a", "refs/heads/a"));
END_TEST

git_testsuite *libgit2_suite_refs(void)
{
	git_testsuite *suite = git_testsuite_new("References");

	ADD_TEST(suite, "readtag", loose_tag_reference_looking_up);
	ADD_TEST(suite, "readtag", non_existing_tag_reference_looking_up);
	ADD_TEST(suite, "readsymref", symbolic_reference_looking_up);
	ADD_TEST(suite, "readsymref", nested_symbolic_reference_looking_up);
	ADD_TEST(suite, "readsymref", looking_up_head_then_master);
	ADD_TEST(suite, "readsymref", looking_up_master_then_head);
	ADD_TEST(suite, "readpackedref", packed_reference_looking_up);
	ADD_TEST(suite, "readpackedref", packed_exists_but_more_recent_loose_reference_is_retrieved);
	ADD_TEST(suite, "createref", create_new_symbolic_ref);
	ADD_TEST(suite, "createref", create_new_object_id_ref);
	ADD_TEST(suite, "normalizeref", normalize_unknown_ref_type);
	ADD_TEST(suite, "normalizeref", normalize_object_id_ref);
	ADD_TEST(suite, "normalizeref", normalize_symbolic_ref);
	ADD_TEST(suite, "normalizeref", normalize_any_ref);

	return suite;
}
