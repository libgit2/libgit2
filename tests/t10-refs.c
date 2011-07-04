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

static const char *loose_tag_ref_name = "refs/tags/e90810b";
static const char *non_existing_tag_ref_name = "refs/tags/i-do-not-exist";

BEGIN_TEST(readtag0, "lookup a loose tag reference")
	git_repository *repo;
	git_reference *reference;
	git_object *object;
	char ref_name_from_tag_name[GIT_REFNAME_MAX];

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&reference, repo, loose_tag_ref_name));
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);

	must_pass(git_object_lookup(&object, repo, git_reference_oid(reference), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_TAG);

	/* Ensure the name of the tag matches the name of the reference */
	git_path_join(ref_name_from_tag_name, GIT_REFS_TAGS_DIR, git_tag_name((git_tag *)object));
	must_be_true(strcmp(ref_name_from_tag_name, loose_tag_ref_name) == 0);

	git_object_close(object);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(readtag1, "lookup a loose tag reference that doesn't exist")
	git_repository *repo;
	git_reference *reference;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_fail(git_reference_lookup(&reference, repo, non_existing_tag_ref_name));

	git_repository_free(repo);
END_TEST

static const char *head_tracker_sym_ref_name = "head-tracker";
static const char *current_head_target = "refs/heads/master";
static const char *current_master_tip = "be3563ae3f795b2b4353bcce3a527ad0a4f7f644";

BEGIN_TEST(readsym0, "lookup a symbolic reference")
	git_repository *repo;
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&reference, repo, GIT_HEAD_FILE));
	must_be_true(reference->type & GIT_REF_SYMBOLIC);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, GIT_HEAD_FILE) == 0);

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	must_pass(git_object_lookup(&object, repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_fromstr(&id, current_master_tip);
	must_be_true(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_object_close(object);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(readsym1, "lookup a nested symbolic reference")
	git_repository *repo;
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&reference, repo, head_tracker_sym_ref_name));
	must_be_true(reference->type & GIT_REF_SYMBOLIC);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, head_tracker_sym_ref_name) == 0);

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	must_pass(git_object_lookup(&object, repo, git_reference_oid(resolved_ref), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_oid_fromstr(&id, current_master_tip);
	must_be_true(git_oid_cmp(&id, git_object_id(object)) == 0);

	git_object_close(object);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(readsym2, "lookup the HEAD and resolve the master branch")
	git_repository *repo;
	git_reference *reference, *resolved_ref, *comp_base_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&reference, repo, head_tracker_sym_ref_name));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	comp_base_ref = resolved_ref;

	must_pass(git_reference_lookup(&reference, repo, GIT_HEAD_FILE));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));

	must_pass(git_reference_lookup(&reference, repo, current_head_target));
	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(comp_base_ref), git_reference_oid(resolved_ref)));

	git_repository_free(repo);
END_TEST

BEGIN_TEST(readsym3, "lookup the master branch and then the HEAD")
	git_repository *repo;
	git_reference *reference, *master_ref, *resolved_ref;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&master_ref, repo, current_head_target));
	must_pass(git_reference_lookup(&reference, repo, GIT_HEAD_FILE));

	must_pass(git_reference_resolve(&resolved_ref, reference));
	must_pass(git_oid_cmp(git_reference_oid(master_ref), git_reference_oid(resolved_ref)));

	git_repository_free(repo);
END_TEST

static const char *packed_head_name = "refs/heads/packed";
static const char *packed_test_head_name = "refs/heads/packed-test";

BEGIN_TEST(readpacked0, "lookup a packed reference")
	git_repository *repo;
	git_reference *reference;
	git_object *object;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&reference, repo, packed_head_name));
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) != 0);
	must_be_true(strcmp(reference->name, packed_head_name) == 0);

	must_pass(git_object_lookup(&object, repo, git_reference_oid(reference), GIT_OBJ_ANY));
	must_be_true(object != NULL);
	must_be_true(git_object_type(object) == GIT_OBJ_COMMIT);

	git_object_close(object);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(readpacked1, "assure that a loose reference is looked up before a packed reference")
	git_repository *repo;
	git_reference *reference;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_reference_lookup(&reference, repo, packed_head_name));
	must_pass(git_reference_lookup(&reference, repo, packed_test_head_name));
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, packed_test_head_name) == 0);

	git_repository_free(repo);
END_TEST

BEGIN_TEST(create0, "create a new symbolic reference")
	git_reference *new_reference, *looked_up_ref, *resolved_ref;
	git_repository *repo, *repo2;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head_tracker = "another-head-tracker";

	git_oid_fromstr(&id, current_master_tip);

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Retrieve the physical path to the symbolic ref for further cleaning */
	git_path_join(ref_path, repo->path_repository, new_head_tracker);

	/* Create and write the new symbolic reference */
	must_pass(git_reference_create_symbolic(&new_reference, repo, new_head_tracker, current_head_target, 0));

	/* Ensure the reference can be looked-up... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, new_head_tracker));
	must_be_true(looked_up_ref->type & GIT_REF_SYMBOLIC);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(looked_up_ref->name, new_head_tracker) == 0);

	/* ...peeled.. */
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(resolved_ref->type == GIT_REF_OID);

	/* ...and that it points to the current master tip */
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	git_repository_free(repo);

	/* Similar test with a fresh new repository */
	must_pass(git_repository_open(&repo2, TEMP_REPO_FOLDER));

	must_pass(git_reference_lookup(&looked_up_ref, repo2, new_head_tracker));
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	close_temp_repo(repo2);
END_TEST

BEGIN_TEST(create1, "create a deep symbolic reference")
	git_reference *new_reference, *looked_up_ref, *resolved_ref;
	git_repository *repo;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head_tracker = "deep/rooted/tracker";

	git_oid_fromstr(&id, current_master_tip);

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	git_path_join(ref_path, repo->path_repository, new_head_tracker);
	must_pass(git_reference_create_symbolic(&new_reference, repo, new_head_tracker, current_head_target, 0));
	must_pass(git_reference_lookup(&looked_up_ref, repo, new_head_tracker));
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(create2, "create a new OID reference")
	git_reference *new_reference, *looked_up_ref;
	git_repository *repo, *repo2;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head = "refs/heads/new-head";

	git_oid_fromstr(&id, current_master_tip);

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Retrieve the physical path to the symbolic ref for further cleaning */
	git_path_join(ref_path, repo->path_repository, new_head);

	/* Create and write the new object id reference */
	must_pass(git_reference_create_oid(&new_reference, repo, new_head, &id, 0));

	/* Ensure the reference can be looked-up... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, new_head));
	must_be_true(looked_up_ref->type & GIT_REF_OID);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(looked_up_ref->name, new_head) == 0);

	/* ...and that it points to the current master tip */
	must_be_true(git_oid_cmp(&id, git_reference_oid(looked_up_ref)) == 0);

	git_repository_free(repo);

	/* Similar test with a fresh new repository */
	must_pass(git_repository_open(&repo2, TEMP_REPO_FOLDER));

	must_pass(git_reference_lookup(&looked_up_ref, repo2, new_head));
	must_be_true(git_oid_cmp(&id, git_reference_oid(looked_up_ref)) == 0);

	close_temp_repo(repo2);
END_TEST

BEGIN_TEST(create3, "Can not create a new OID reference which targets at an unknown id")
	git_reference *new_reference, *looked_up_ref;
	git_repository *repo;
	git_oid id;

	const char *new_head = "refs/heads/new-head";

	git_oid_fromstr(&id, "deadbeef3f795b2b4353bcce3a527ad0a4f7f644");

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* Create and write the new object id reference */
	must_fail(git_reference_create_oid(&new_reference, repo, new_head, &id, 0));

	/* Ensure the reference can't be looked-up... */
	must_fail(git_reference_lookup(&looked_up_ref, repo, new_head));

	git_repository_free(repo);
END_TEST

static const char *ref_name = "refs/heads/other";
static const char *ref_master_name = "refs/heads/master";
static const char *ref_branch_name = "refs/heads/branch";
static const char *ref_test_name = "refs/heads/test";
BEGIN_TEST(overwrite0, "Overwrite an existing symbolic reference")
	git_reference *ref, *branch_ref;
	git_repository *repo;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* The target needds to exist and we need to check the name has changed */
	must_pass(git_reference_create_symbolic(&branch_ref, repo, ref_branch_name, ref_master_name, 0));
	must_pass(git_reference_create_symbolic(&ref, repo, ref_name, ref_branch_name, 0));
	/* Ensure it points to the right place*/
	must_pass(git_reference_lookup(&ref, repo, ref_name));
	must_be_true(git_reference_type(ref) & GIT_REF_SYMBOLIC);
	must_be_true(!strcmp(git_reference_target(ref), ref_branch_name));

	/* Ensure we can't create it unless we force it to */
	must_fail(git_reference_create_symbolic(&ref, repo, ref_name, ref_master_name, 0));
	must_pass(git_reference_create_symbolic(&ref, repo, ref_name, ref_master_name, 1));

	/* Ensure it points to the right place */
	must_pass(git_reference_lookup(&ref, repo, ref_name));
	must_be_true(git_reference_type(ref) & GIT_REF_SYMBOLIC);
	must_be_true(!strcmp(git_reference_target(ref), ref_master_name));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(overwrite1, "Overwrite an existing object id reference")
	git_reference *ref;
	git_repository *repo;
	git_oid id;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&ref, repo, ref_master_name));
	must_be_true(ref->type & GIT_REF_OID);
	git_oid_cpy(&id, git_reference_oid(ref));

	/* Create it */
	must_pass(git_reference_create_oid(&ref, repo, ref_name, &id, 0));

	must_pass(git_reference_lookup(&ref, repo, ref_test_name));
	must_be_true(ref->type & GIT_REF_OID);
	git_oid_cpy(&id, git_reference_oid(ref));

	/* Ensure we can't overwrite unless we force it */
	must_fail(git_reference_create_oid(&ref, repo, ref_name, &id, 0));
	must_pass(git_reference_create_oid(&ref, repo, ref_name, &id, 1));

	/* Ensure it has been overwritten */
	must_pass(git_reference_lookup(&ref, repo, ref_name));
	must_be_true(!git_oid_cmp(&id, git_reference_oid(ref)));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(overwrite2, "Overwrite an existing object id reference with a symbolic one")
	git_reference *ref;
	git_repository *repo;
	git_oid id;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&ref, repo, ref_master_name));
	must_be_true(ref->type & GIT_REF_OID);
	git_oid_cpy(&id, git_reference_oid(ref));

	must_pass(git_reference_create_oid(&ref, repo, ref_name, &id, 0));
	must_fail(git_reference_create_symbolic(&ref, repo, ref_name, ref_master_name, 0));
	must_pass(git_reference_create_symbolic(&ref, repo, ref_name, ref_master_name, 1));

	/* Ensure it points to the right place */
	must_pass(git_reference_lookup(&ref, repo, ref_name));
	must_be_true(git_reference_type(ref) & GIT_REF_SYMBOLIC);
	must_be_true(!strcmp(git_reference_target(ref), ref_master_name));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(overwrite3, "Overwrite an existing symbolic reference with an object id one")
	git_reference *ref;
	git_repository *repo;
	git_oid id;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&ref, repo, ref_master_name));
	must_be_true(ref->type & GIT_REF_OID);
	git_oid_cpy(&id, git_reference_oid(ref));

	/* Create the symbolic ref */
	must_pass(git_reference_create_symbolic(&ref, repo, ref_name, ref_master_name, 0));
	/* It shouldn't overwrite unless we tell it to */
	must_fail(git_reference_create_oid(&ref, repo, ref_name, &id, 0));
	must_pass(git_reference_create_oid(&ref, repo, ref_name, &id, 1));

	/* Ensure it points to the right place */
	must_pass(git_reference_lookup(&ref, repo, ref_name));
	must_be_true(git_reference_type(ref) & GIT_REF_OID);
	must_be_true(!git_oid_cmp(git_reference_oid(ref), &id));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(pack0, "create a packfile for an empty folder")
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];
	const int mode = 0755; /* or 0777 ? */

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	git_path_join_n(temp_path, 3, repo->path_repository, GIT_REFS_HEADS_DIR, "empty_dir");
	must_pass(git_futils_mkdir_r(temp_path, mode));

	must_pass(git_reference_packall(repo));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(pack1, "create a packfile from all the loose rn a repo")
	git_repository *repo;
	git_reference *reference;
	char temp_path[GIT_PATH_MAX];

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Ensure a known loose ref can be looked up */
	must_pass(git_reference_lookup(&reference, repo, loose_tag_ref_name));
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);

	/*
	 * We are now trying to pack also a loose reference
	 * called `points_to_blob`, to make sure we can properly
	 * pack weak tags
	 */
	must_pass(git_reference_packall(repo));

	/* Ensure the packed-refs file exists */
	git_path_join(temp_path, repo->path_repository, GIT_PACKEDREFS_FILE);
	must_pass(git_futils_exists(temp_path));

	/* Ensure the known ref can still be looked up but is now packed */
	must_pass(git_reference_lookup(&reference, repo, loose_tag_ref_name));
	must_be_true((reference->type & GIT_REF_PACKED) != 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);

	/* Ensure the known ref has been removed from the loose folder structure */
	git_path_join(temp_path, repo->path_repository, loose_tag_ref_name);
	must_pass(!git_futils_exists(temp_path));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename0, "rename a loose reference")
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];
	const char *new_name = "refs/tags/Nemo/knows/refs.kung-fu";

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Ensure the ref doesn't exist on the file system */
	git_path_join(temp_path, repo->path_repository, new_name);
	must_pass(!git_futils_exists(temp_path));

	/* Retrieval of the reference to rename */
	must_pass(git_reference_lookup(&looked_up_ref, repo, loose_tag_ref_name));

	/* ... which is indeed loose */
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* Now that the reference is renamed... */
	must_pass(git_reference_rename(looked_up_ref, new_name, 0));
	must_be_true(!strcmp(looked_up_ref->name, new_name));

	/* ...It can't be looked-up with the old name... */
	must_fail(git_reference_lookup(&another_looked_up_ref, repo, loose_tag_ref_name));

	/* ...but the new name works ok... */
	must_pass(git_reference_lookup(&another_looked_up_ref, repo, new_name));
	must_be_true(!strcmp(another_looked_up_ref->name, new_name));

	/* .. the ref is still loose... */
	must_be_true((another_looked_up_ref->type & GIT_REF_PACKED) == 0);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* ...and the ref can be found in the file system */
	git_path_join(temp_path, repo->path_repository, new_name);
	must_pass(git_futils_exists(temp_path));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename1, "rename a packed reference (should make it loose)")
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];
	const char *brand_new_name = "refs/heads/brand_new_name";

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Ensure the ref doesn't exist on the file system */
	git_path_join(temp_path, repo->path_repository, packed_head_name);
	must_pass(!git_futils_exists(temp_path));

	/* The reference can however be looked-up... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_head_name));

	/* .. and it's packed */
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) != 0);

	/* Now that the reference is renamed... */
	must_pass(git_reference_rename(looked_up_ref, brand_new_name, 0));
	must_be_true(!strcmp(looked_up_ref->name, brand_new_name));

	/* ...It can't be looked-up with the old name... */
	must_fail(git_reference_lookup(&another_looked_up_ref, repo, packed_head_name));

	/* ...but the new name works ok... */
	must_pass(git_reference_lookup(&another_looked_up_ref, repo, brand_new_name));
	must_be_true(!strcmp(another_looked_up_ref->name, brand_new_name));

	/* .. the ref is no longer packed... */
	must_be_true((another_looked_up_ref->type & GIT_REF_PACKED) == 0);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* ...and the ref now happily lives in the file system */
	git_path_join(temp_path, repo->path_repository, brand_new_name);
	must_pass(git_futils_exists(temp_path));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename2, "renaming a packed reference does not pack another reference which happens to be in both loose and pack state")
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];
	const char *brand_new_name = "refs/heads/brand_new_name";

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Ensure the other reference exists on the file system */
	git_path_join(temp_path, repo->path_repository, packed_test_head_name);
	must_pass(git_futils_exists(temp_path));

	/* Lookup the other reference */
	must_pass(git_reference_lookup(&another_looked_up_ref, repo, packed_test_head_name));

	/* Ensure it's loose */
	must_be_true((another_looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* Lookup the reference to rename */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_head_name));

	/* Ensure it's packed */
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) != 0);

	/* Now that the reference is renamed... */
	must_pass(git_reference_rename(looked_up_ref, brand_new_name, 0));

	/* Lookup the other reference */
	must_pass(git_reference_lookup(&another_looked_up_ref, repo, packed_test_head_name));

	/* Ensure it's loose */
	must_be_true((another_looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* Ensure the other ref still exists on the file system */
	must_pass(git_futils_exists(temp_path));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename3, "can not rename a reference with the name of an existing reference")
	git_reference *looked_up_ref;
	git_repository *repo;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* An existing reference... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_head_name));

	/* Can not be renamed to the name of another existing reference. */
	must_fail(git_reference_rename(looked_up_ref, packed_test_head_name, 0));

	/* Failure to rename it hasn't corrupted its state */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_head_name));
	must_be_true(!strcmp(looked_up_ref->name, packed_head_name));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename4, "can not rename a reference with an invalid name")
	git_reference *looked_up_ref;
	git_repository *repo;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* An existing oid reference... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_test_head_name));

	/* Can not be renamed with an invalid name. */
	must_fail(git_reference_rename(looked_up_ref, "Hello! I'm a very invalid name.", 0));

	/* Can not be renamed outside of the refs hierarchy. */
	must_fail(git_reference_rename(looked_up_ref, "i-will-sudo-you", 0));

	/* Failure to rename it hasn't corrupted its state */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_test_head_name));
	must_be_true(!strcmp(looked_up_ref->name, packed_test_head_name));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(rename5, "can force-rename a reference with the name of an existing reference")
	git_reference *looked_up_ref;
	git_repository *repo;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* An existing reference... */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_head_name));

	/* Can be force-renamed to the name of another existing reference. */
	must_pass(git_reference_rename(looked_up_ref, packed_test_head_name, 1));

	/* Check we actually renamed it */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_test_head_name));
	must_be_true(!strcmp(looked_up_ref->name, packed_test_head_name));

	close_temp_repo(repo);
END_TEST

static const char *ref_one_name = "refs/heads/one/branch";
static const char *ref_one_name_new = "refs/heads/two/branch";
static const char *ref_two_name = "refs/heads/two";

BEGIN_TEST(rename6, "can not overwrite name of existing reference")
	git_reference *ref, *ref_one, *ref_one_new, *ref_two;
	git_repository *repo;
	git_oid id;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	must_pass(git_reference_lookup(&ref, repo, ref_master_name));
	must_be_true(ref->type & GIT_REF_OID);

	git_oid_cpy(&id, git_reference_oid(ref));

	/* Create loose references */
	must_pass(git_reference_create_oid(&ref_one, repo, ref_one_name, &id, 0));
	must_pass(git_reference_create_oid(&ref_two, repo, ref_two_name, &id, 0));

	/* Pack everything */
	must_pass(git_reference_packall(repo));

	/* Attempt to create illegal reference */
	must_fail(git_reference_create_oid(&ref_one_new, repo, ref_one_name_new, &id, 0));

	/* Illegal reference couldn't be created so this is supposed to fail */
	must_fail(git_reference_lookup(&ref_one_new, repo, ref_one_name_new));

	close_temp_repo(repo);
END_TEST

BEGIN_TEST(delete0, "deleting a ref which is both packed and loose should remove both tracks in the filesystem")
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	/* Ensure the loose reference exists on the file system */
	git_path_join(temp_path, repo->path_repository, packed_test_head_name);
	must_pass(git_futils_exists(temp_path));

	/* Lookup the reference */
	must_pass(git_reference_lookup(&looked_up_ref, repo, packed_test_head_name));

	/* Ensure it's the loose version that has been found */
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* Now that the reference is deleted... */
	must_pass(git_reference_delete(looked_up_ref));

	/* Looking up the reference once again should not retrieve it */
	must_fail(git_reference_lookup(&another_looked_up_ref, repo, packed_test_head_name));

	/* Ensure the loose reference doesn't exist any longer on the file system */
	must_pass(!git_futils_exists(temp_path));

	close_temp_repo(repo);
END_TEST

static int ensure_refname_normalized(int is_oid_ref, const char *input_refname, const char *expected_refname)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_REFNAME_MAX];

	if (is_oid_ref)
		error = git_reference__normalize_name_oid(buffer_out, sizeof(buffer_out), input_refname);
	else
		error = git_reference__normalize_name(buffer_out, sizeof(buffer_out), input_refname);

	if (error < GIT_SUCCESS)
		return error;

	if (expected_refname == NULL)
		return error;

	if (strcmp(buffer_out, expected_refname))
		error = GIT_ERROR;

	return error;
}

#define OID_REF 1
#define SYM_REF 0

BEGIN_TEST(normalize0, "normalize a direct (OID) reference name")
	must_fail(ensure_refname_normalized(OID_REF, "a", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a/", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.lock", NULL));
	must_pass(ensure_refname_normalized(OID_REF, "refs/dummy/a", NULL));
	must_pass(ensure_refname_normalized(OID_REF, "refs/stash", NULL));
	must_pass(ensure_refname_normalized(OID_REF, "refs/tags/a", "refs/tags/a"));
	must_pass(ensure_refname_normalized(OID_REF, "refs/heads/a/b", "refs/heads/a/b"));
	must_pass(ensure_refname_normalized(OID_REF, "refs/heads/a./b", "refs/heads/a./b"));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo?bar", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads\foo", NULL));
	must_pass(ensure_refname_normalized(OID_REF, "refs/heads/v@ation", "refs/heads/v@ation"));
	must_pass(ensure_refname_normalized(OID_REF, "refs///heads///a", "refs/heads/a"));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/.a/b", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo/../bar", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/foo..bar", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/./foo", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/v@{ation", NULL));
END_TEST

BEGIN_TEST(normalize1, "normalize a symbolic reference name")
	must_pass(ensure_refname_normalized(SYM_REF, "a", "a"));
	must_pass(ensure_refname_normalized(SYM_REF, "a/b", "a/b"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs///heads///a", "refs/heads/a"));
	must_fail(ensure_refname_normalized(SYM_REF, "", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "heads\foo", NULL));
END_TEST

/* Ported from JGit, BSD licence.
 * See https://github.com/spearce/JGit/commit/e4bf8f6957bbb29362575d641d1e77a02d906739 */
BEGIN_TEST(normalize2, "tests borrowed from JGit")

/* EmptyString */
	must_fail(ensure_refname_normalized(SYM_REF, "", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "/", NULL));

/* MustHaveTwoComponents */
	must_fail(ensure_refname_normalized(OID_REF, "master", NULL));
	must_pass(ensure_refname_normalized(SYM_REF, "heads/master", "heads/master"));

/* ValidHead */

	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/master", "refs/heads/master"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/pu", "refs/heads/pu"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/z", "refs/heads/z"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/FoO", "refs/heads/FoO"));

/* ValidTag */
	must_pass(ensure_refname_normalized(SYM_REF, "refs/tags/v1.0", "refs/tags/v1.0"));

/* NoLockSuffix */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master.lock", NULL));

/* NoDirectorySuffix */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master/", NULL));

/* NoSpace */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/i haz space", NULL));

/* NoAsciiControlCharacters */
	{
		char c;
		char buffer[GIT_REFNAME_MAX];
		for (c = '\1'; c < ' '; c++) {
			strncpy(buffer, "refs/heads/mast", 15);
			strncpy(buffer + 15, (const char *)&c, 1);
			strncpy(buffer + 16, "er", 2);
			buffer[18 - 1] = '\0';
			must_fail(ensure_refname_normalized(SYM_REF, buffer, NULL));
		}
	}

/* NoBareDot */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/.", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/..", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/./master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/../master", NULL));

/* NoLeadingOrTrailingDot */
	must_fail(ensure_refname_normalized(SYM_REF, ".", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/.bar", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/..bar", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/bar.", NULL));

/* ContainsDot */
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/m.a.s.t.e.r", "refs/heads/m.a.s.t.e.r"));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master..pu", NULL));

/* NoMagicRefCharacters */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master^", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/^master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "^refs/heads/master", NULL));

	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master~", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/~master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "~refs/heads/master", NULL));

	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master:", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/:master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, ":refs/heads/master", NULL));

/* ShellGlob */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master?", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/?master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "?refs/heads/master", NULL));

	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master[", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/[master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "[refs/heads/master", NULL));

	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master*", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/*master", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "*refs/heads/master", NULL));

/* ValidSpecialCharacters */
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/!", "refs/heads/!"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/\"", "refs/heads/\""));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/#", "refs/heads/#"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/$", "refs/heads/$"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/%", "refs/heads/%"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/&", "refs/heads/&"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/'", "refs/heads/'"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/(", "refs/heads/("));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/)", "refs/heads/)"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/+", "refs/heads/+"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/,", "refs/heads/,"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/-", "refs/heads/-"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/;", "refs/heads/;"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/<", "refs/heads/<"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/=", "refs/heads/="));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/>", "refs/heads/>"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/@", "refs/heads/@"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/]", "refs/heads/]"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/_", "refs/heads/_"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/`", "refs/heads/`"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/{", "refs/heads/{"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/|", "refs/heads/|"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/}", "refs/heads/}"));

	// This is valid on UNIX, but not on Windows
	// hence we make in invalid due to non-portability
	//
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/\\", NULL));

/* UnicodeNames */
	/*
	 * Currently this fails.
	 * must_pass(ensure_refname_normalized(SYM_REF, "refs/heads/\u00e5ngstr\u00f6m", "refs/heads/\u00e5ngstr\u00f6m"));
	 */

/* RefLogQueryIsValidRef */
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master@{1}", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "refs/heads/master@{1.hour.ago}", NULL));
END_TEST

BEGIN_TEST(list0, "try to list all the references in our test repo")
	git_repository *repo;
	git_strarray ref_list;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_reference_listall(&ref_list, repo, GIT_REF_LISTALL));

	/*{
		unsigned short i;
		for (i = 0; i < ref_list.count; ++i)
			printf("# %s\n", ref_list.strings[i]);
	}*/

	/* We have exactly 8 refs in total if we include the packed ones:
	 * there is a reference that exists both in the packfile and as
	 * loose, but we only list it once */
	must_be_true(ref_list.count == 8);

	git_strarray_free(&ref_list);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(list1, "try to list only the symbolic references")
	git_repository *repo;
	git_strarray ref_list;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_reference_listall(&ref_list, repo, GIT_REF_SYMBOLIC));
	must_be_true(ref_list.count == 0); /* no symrefs in the test repo */

	git_strarray_free(&ref_list);
	git_repository_free(repo);
END_TEST


BEGIN_SUITE(refs)
	ADD_TEST(readtag0);
	ADD_TEST(readtag1);

	ADD_TEST(readsym0);
	ADD_TEST(readsym1);
	ADD_TEST(readsym2);
	ADD_TEST(readsym3);

	ADD_TEST(readpacked0);
	ADD_TEST(readpacked1);

	ADD_TEST(create0);
	ADD_TEST(create1);
	ADD_TEST(create2);
	ADD_TEST(create3);

	ADD_TEST(overwrite0);
	ADD_TEST(overwrite1);
	ADD_TEST(overwrite2);
	ADD_TEST(overwrite3);

	ADD_TEST(normalize0);
	ADD_TEST(normalize1);
	ADD_TEST(normalize2);

	ADD_TEST(pack0);
	ADD_TEST(pack1);

	ADD_TEST(rename0);
	ADD_TEST(rename1);
	ADD_TEST(rename2);
	ADD_TEST(rename3);
	ADD_TEST(rename4);
	ADD_TEST(rename5);
	ADD_TEST(rename6);

	ADD_TEST(delete0);
	ADD_TEST(list0);
	ADD_TEST(list1);
END_SUITE
