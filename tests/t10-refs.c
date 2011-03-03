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
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
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

static const char *head_tracker_sym_ref_name = "head-tracker";
static const char *current_head_target = "refs/heads/master";
static const char *current_master_tip = "be3563ae3f795b2b4353bcce3a527ad0a4f7f644";

BEGIN_TEST("readsymref", symbolic_reference_looking_up)
	git_repository *repo;
	git_reference *reference, *resolved_ref;
	git_object *object;
	git_oid id;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&reference, repo, GIT_HEAD_FILE));
	must_be_true(reference->type & GIT_REF_SYMBOLIC);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, GIT_HEAD_FILE) == 0);

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
	must_be_true(reference->type & GIT_REF_SYMBOLIC);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
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

	must_pass(git_repository_lookup_ref(&reference, repo, GIT_HEAD_FILE));
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
	must_pass(git_repository_lookup_ref(&reference, repo, GIT_HEAD_FILE));

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
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) != 0);
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
	must_be_true(reference->type & GIT_REF_OID);
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
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
	must_pass(git_reference_create_symbolic(&new_reference, repo, new_head_tracker, current_head_target));

	/* Ensure the reference can be looked-up... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head_tracker));
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
	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head_tracker));
	must_pass(git_reference_resolve(&resolved_ref, looked_up_ref));
	must_be_true(git_oid_cmp(&id, git_reference_oid(resolved_ref)) == 0);

	git_repository_free(repo);

	must_pass(gitfo_unlink(ref_path));	/* TODO: replace with git_reference_delete() when available */
END_TEST

BEGIN_TEST("createref", create_deep_symbolic_ref)
	git_reference *new_reference, *looked_up_ref, *resolved_ref;
	git_repository *repo;
	git_oid id;
	char ref_path[GIT_PATH_MAX];

	const char *new_head_tracker = "deep/rooted/tracker";

	git_oid_mkstr(&id, current_master_tip);

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git__joinpath(ref_path, repo->path_repository, new_head_tracker);
	must_pass(git_reference_create_symbolic(&new_reference, repo, new_head_tracker, current_head_target));
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
	must_pass(git_reference_create_oid(&new_reference, repo, new_head, &id));

	/* Ensure the reference can be looked-up... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, new_head));
	must_be_true(looked_up_ref->type & GIT_REF_OID);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);
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

BEGIN_TEST("packrefs", create_packfile_with_empty_folder)
	git_repository *repo;
	git_reference *reference;
	char temp_path[GIT_PATH_MAX];
	int path_len = 0;
	const int mode = 0755; /* or 0777 ? */

	must_pass(copydir_recurs(REPOSITORY_FOLDER, TEMP_DIR));

	git__joinpath(temp_path, TEMP_DIR, TEST_REPOSITORY_NAME);
	must_pass(git_repository_open(&repo, temp_path));
	
	git__joinpath_n(temp_path, 3, repo->path_repository, GIT_REFS_HEADS_DIR, "empty_dir");
	must_pass(gitfo_mkdir_recurs(temp_path, mode));

	must_pass(git_reference_packall(repo));

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_DIR));
END_TEST

BEGIN_TEST("packrefs", create_packfile)
	git_repository *repo;
	git_reference *reference;
	char temp_path[GIT_PATH_MAX];
	int path_len = 0;

	must_pass(copydir_recurs(REPOSITORY_FOLDER, TEMP_DIR));

	git__joinpath(temp_path, TEMP_DIR, TEST_REPOSITORY_NAME);
	must_pass(git_repository_open(&repo, temp_path));
	
	/* Ensure a known loose ref can be looked up */
	must_pass(git_repository_lookup_ref(&reference, repo, loose_tag_ref_name));
	must_be_true((reference->type & GIT_REF_PACKED) == 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);
	
	must_pass(git_reference_packall(repo));

	/* Ensure the packed-refs file exists */
	git__joinpath(temp_path, repo->path_repository, GIT_PACKEDREFS_FILE);
	must_pass(gitfo_exists(temp_path));

	/* Ensure the known ref can still be looked up but is now packed */
	must_pass(git_repository_lookup_ref(&reference, repo, loose_tag_ref_name));
	must_be_true((reference->type & GIT_REF_PACKED) != 0);
	must_be_true(strcmp(reference->name, loose_tag_ref_name) == 0);

	/* Ensure the known ref has been removed from the loose folder structure */
	git__joinpath(temp_path, repo->path_repository, loose_tag_ref_name);
	must_pass(!gitfo_exists(temp_path));

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_DIR));
END_TEST

BEGIN_TEST("renameref", renaming_a_packed_reference_makes_it_loose)
	git_reference *looked_up_ref, *another_looked_up_ref;
	git_repository *repo;
	char temp_path[GIT_PATH_MAX];
	const char *brand_new_name = "refs/heads/brand_new_name";

	must_pass(copydir_recurs(REPOSITORY_FOLDER, TEMP_DIR));

	git__joinpath(temp_path, TEMP_DIR, TEST_REPOSITORY_NAME);
	must_pass(git_repository_open(&repo, temp_path));

	/* Ensure the ref doesn't exist on the file system */
	git__joinpath(temp_path, repo->path_repository, packed_head_name);
	must_pass(!gitfo_exists(temp_path));

	/* The reference can however be looked-up... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, packed_head_name));

	/* .. and it's packed */
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) != 0);

	/* Now that the reference is renamed... */
	must_pass(git_reference_rename(looked_up_ref, brand_new_name));
	must_be_true(!strcmp(looked_up_ref->name, brand_new_name));

	/* ...It can't be looked-up with the old name... */
	must_fail(git_repository_lookup_ref(&another_looked_up_ref, repo, packed_head_name));

	/* ...but the new name works ok... */
	must_pass(git_repository_lookup_ref(&another_looked_up_ref, repo, brand_new_name));
	must_be_true(!strcmp(another_looked_up_ref->name, brand_new_name));

	/* .. the ref is no longer packed... */
	must_be_true((another_looked_up_ref->type & GIT_REF_PACKED) == 0);
	must_be_true((looked_up_ref->type & GIT_REF_PACKED) == 0);

	/* ...and the ref now happily lives in the file system */
	git__joinpath(temp_path, repo->path_repository, brand_new_name);
	must_pass(gitfo_exists(temp_path));

	git_repository_free(repo);

	must_pass(rmdir_recurs(TEMP_DIR));
END_TEST

BEGIN_TEST("renameref", can_not_rename_a_reference_with_the_name_of_an_existing_reference)
	git_reference *looked_up_ref;
	git_repository *repo;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* An existing reference... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, packed_head_name));

	/* Can not be renamed to the name of another existing reference. */
	must_fail(git_reference_rename(looked_up_ref, packed_test_head_name));

	/* Failure to rename it hasn't corrupted its state */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, packed_head_name));
	must_be_true(!strcmp(looked_up_ref->name, packed_head_name));

	git_repository_free(repo);
END_TEST

BEGIN_TEST("renameref", can_not_rename_a_reference_with_an_invalid_name)
	git_reference *looked_up_ref;
	git_repository *repo;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	/* An existing oid reference... */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, packed_test_head_name));

	/* Can not be renamed with an invalid name. */
	must_fail(git_reference_rename(looked_up_ref, "Hello! I'm a very invalid name."));

	/* Can not be renamed outside of the refs hierarchy. */
	must_fail(git_reference_rename(looked_up_ref, "i-will-sudo-you"));

	/* Failure to rename it hasn't corrupted its state */
	must_pass(git_repository_lookup_ref(&looked_up_ref, repo, packed_test_head_name));
	must_be_true(!strcmp(looked_up_ref->name, packed_test_head_name));

	git_repository_free(repo);
END_TEST

static int ensure_refname_normalized(int is_oid_ref, const char *input_refname, const char *expected_refname)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_PATH_MAX];

	if (is_oid_ref)
		error = git_reference__normalize_name_oid(buffer_out, input_refname);
	else
		error = git_reference__normalize_name(buffer_out, input_refname);

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

BEGIN_TEST("normalizeref", normalize_object_id_ref)
	must_fail(ensure_refname_normalized(OID_REF, "a", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a/", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/heads/a.lock", NULL));
	must_fail(ensure_refname_normalized(OID_REF, "refs/dummy/a", NULL));
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

BEGIN_TEST("normalizeref", normalize_symbolic_ref)
	must_pass(ensure_refname_normalized(SYM_REF, "a", "a"));
	must_pass(ensure_refname_normalized(SYM_REF, "a/b", "a/b"));
	must_pass(ensure_refname_normalized(SYM_REF, "refs///heads///a", "refs/heads/a"));
	must_fail(ensure_refname_normalized(SYM_REF, "", NULL));
	must_fail(ensure_refname_normalized(SYM_REF, "heads\foo", NULL));
END_TEST

/* Ported from JGit, BSD licence. See https://github.com/spearce/JGit/commit/e4bf8f6957bbb29362575d641d1e77a02d906739 */
BEGIN_TEST("normalizeref", jgit_tests)

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
				char buffer[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];
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
	ADD_TEST(suite, "createref", create_deep_symbolic_ref);
	ADD_TEST(suite, "createref", create_new_object_id_ref);
	ADD_TEST(suite, "normalizeref", normalize_object_id_ref);
	ADD_TEST(suite, "normalizeref", normalize_symbolic_ref);
	ADD_TEST(suite, "normalizeref", jgit_tests);
	ADD_TEST(suite, "packrefs", create_packfile_with_empty_folder);
	ADD_TEST(suite, "packrefs", create_packfile);
	ADD_TEST(suite, "renameref", renaming_a_packed_reference_makes_it_loose);
	ADD_TEST(suite, "renameref", can_not_rename_a_reference_with_the_name_of_an_existing_reference);
	ADD_TEST(suite, "renameref", can_not_rename_a_reference_with_an_invalid_name);
	return suite;
}
