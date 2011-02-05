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

#include "refs.h"

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

	return suite;
}
