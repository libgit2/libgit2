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

#include "tag.h"

static const char *tag1_id = "b25fa35b38051e4ae45d4222e795f9df2e43f1d1";
static const char *tag2_id = "7b4384978d2493e851f9cca7858815fac9b10980";
static const char *tagged_commit = "e90810b8df3e80c413d903f631643c716887138d";
static const char *bad_tag_id = "eda9f45a2a98d4c17a09d681d88569fa4ea91755";
static const char *badly_tagged_commit = "e90810b8df3e80c413d903f631643c716887138d";

BEGIN_TEST(read0, "read and parse a tag from the repository")
	git_repository *repo;
	git_tag *tag1, *tag2;
	git_commit *commit;
	git_oid id1, id2, id_commit;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&id1, tag1_id);
	git_oid_fromstr(&id2, tag2_id);
	git_oid_fromstr(&id_commit, tagged_commit);

	must_pass(git_tag_lookup(&tag1, repo, &id1));

	must_be_true(strcmp(git_tag_name(tag1), "test") == 0);
	must_be_true(git_tag_type(tag1) == GIT_OBJ_TAG);

	must_pass(git_tag_target((git_object **)&tag2, tag1));
	must_be_true(tag2 != NULL);

	must_be_true(git_oid_cmp(&id2, git_tag_id(tag2)) == 0);

	must_pass(git_tag_target((git_object **)&commit, tag2));
	must_be_true(commit != NULL);

	must_be_true(git_oid_cmp(&id_commit, git_commit_id(commit)) == 0);

	git_tag_close(tag1);
	git_tag_close(tag2);
	git_commit_close(commit);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(read1, "list all tag names from the repository")
	git_repository *repo;
	git_strarray tag_list;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_tag_list(&tag_list, repo));

	must_be_true(tag_list.count == 3);

	git_strarray_free(&tag_list);
	git_repository_free(repo);
END_TEST

static int ensure_tag_pattern_match(git_repository *repo, const char *pattern, const size_t expected_matches)
{
	git_strarray tag_list;
	int error = GIT_SUCCESS;

	if ((error = git_tag_list_match(&tag_list, pattern, repo)) < GIT_SUCCESS)
		goto exit;

	if (tag_list.count != expected_matches)
		error = GIT_ERROR;

exit:
	git_strarray_free(&tag_list);
	return error;
}

BEGIN_TEST(read2, "list all tag names from the repository matching a specified pattern")
	git_repository *repo;
	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(ensure_tag_pattern_match(repo, "", 3));
	must_pass(ensure_tag_pattern_match(repo, "*", 3));
	must_pass(ensure_tag_pattern_match(repo, "t*", 1));
	must_pass(ensure_tag_pattern_match(repo, "*b", 2));
	must_pass(ensure_tag_pattern_match(repo, "e", 0));
	must_pass(ensure_tag_pattern_match(repo, "e90810b", 1));
	must_pass(ensure_tag_pattern_match(repo, "e90810[ab]", 1));
	git_repository_free(repo);
END_TEST

#define BAD_TAG_REPOSITORY_FOLDER TEST_RESOURCES "/bad_tag.git/"

BEGIN_TEST(read3, "read and parse a tag without a tagger field")
	git_repository *repo;
	git_tag *bad_tag;
	git_commit *commit;
	git_oid id, id_commit;

	must_pass(git_repository_open(&repo, BAD_TAG_REPOSITORY_FOLDER));

	git_oid_fromstr(&id, bad_tag_id);
	git_oid_fromstr(&id_commit, badly_tagged_commit);

	must_pass(git_tag_lookup(&bad_tag, repo, &id));
	must_be_true(bad_tag != NULL);

	must_be_true(strcmp(git_tag_name(bad_tag), "e90810b") == 0);
	must_be_true(git_oid_cmp(&id, git_tag_id(bad_tag)) == 0);
	must_be_true(bad_tag->tagger == NULL);

	must_pass(git_tag_target((git_object **)&commit, bad_tag));
	must_be_true(commit != NULL);

	must_be_true(git_oid_cmp(&id_commit, git_commit_id(commit)) == 0);

	git_tag_close(bad_tag);
	git_commit_close(commit);

	git_repository_free(repo);
END_TEST


#define TAGGER_NAME "Vicent Marti"
#define TAGGER_EMAIL "vicent@github.com"
#define TAGGER_MESSAGE "This is my tag.\n\nThere are many tags, but this one is mine\n"

BEGIN_TEST(write0, "write a tag to the repository and read it again")
	git_repository *repo;
	git_tag *tag;
	git_oid target_id, tag_id;
	git_signature *tagger;
	const git_signature *tagger1;
	git_reference *ref_tag;
	git_object *target;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&target_id, tagged_commit);
	must_pass(git_object_lookup(&target, repo, &target_id, GIT_OBJ_COMMIT));

	/* create signature */
	must_pass(git_signature_new(&tagger, TAGGER_NAME, TAGGER_EMAIL, 123456789, 60));

	must_pass(git_tag_create(
		&tag_id, /* out id */
		repo,
		"the-tag",
		target,
		tagger,
		TAGGER_MESSAGE,
		0));

	git_object_close(target);
	git_signature_free(tagger);

	must_pass(git_tag_lookup(&tag, repo, &tag_id));
	must_be_true(git_oid_cmp(git_tag_target_oid(tag), &target_id) == 0);

	/* Check attributes were set correctly */
	tagger1 = git_tag_tagger(tag);
	must_be_true(tagger1 != NULL);
	must_be_true(strcmp(tagger1->name, TAGGER_NAME) == 0);
	must_be_true(strcmp(tagger1->email, TAGGER_EMAIL) == 0);
	must_be_true(tagger1->when.time == 123456789);
	must_be_true(tagger1->when.offset == 60);

	must_be_true(strcmp(git_tag_message(tag), TAGGER_MESSAGE) == 0);

	must_pass(git_reference_lookup(&ref_tag, repo, "refs/tags/the-tag"));
	must_be_true(git_oid_cmp(git_reference_oid(ref_tag), &tag_id) == 0);
	must_pass(git_reference_delete(ref_tag));
#ifndef GIT_WIN32
	must_be_true((loose_object_mode(REPOSITORY_FOLDER, (git_object *)tag) & 0777) == GIT_OBJECT_FILE_MODE);
#endif

	must_pass(remove_loose_object(REPOSITORY_FOLDER, (git_object *)tag));

	git_tag_close(tag);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(write2, "Attempt to write a tag bearing the same name than an already existing tag")
	git_repository *repo;
	git_oid target_id, tag_id;
	git_signature *tagger;
	git_object *target;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&target_id, tagged_commit);
	must_pass(git_object_lookup(&target, repo, &target_id, GIT_OBJ_COMMIT));

	/* create signature */
	must_pass(git_signature_new(&tagger, TAGGER_NAME, TAGGER_EMAIL, 123456789, 60));

	must_fail(git_tag_create(
		&tag_id, /* out id */
		repo,
		"e90810b",
		target,
		tagger,
		TAGGER_MESSAGE,
		0));

	git_object_close(target);
	git_signature_free(tagger);

	git_repository_free(repo);

END_TEST

BEGIN_TEST(write3, "Replace an already existing tag")
	git_repository *repo;
	git_oid target_id, tag_id, old_tag_id;
	git_signature *tagger;
	git_reference *ref_tag;
	git_object *target;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&target_id, tagged_commit);
	must_pass(git_object_lookup(&target, repo, &target_id, GIT_OBJ_COMMIT));

	must_pass(git_reference_lookup(&ref_tag, repo, "refs/tags/e90810b"));
	git_oid_cpy(&old_tag_id, git_reference_oid(ref_tag));

	/* create signature */
	must_pass(git_signature_new(&tagger, TAGGER_NAME, TAGGER_EMAIL, 123456789, 60));

	must_pass(git_tag_create(
		&tag_id, /* out id */
		repo,
		"e90810b",
		target,
		tagger,
		TAGGER_MESSAGE,
		1));

	git_object_close(target);
	git_signature_free(tagger);

	must_pass(git_reference_lookup(&ref_tag, repo, "refs/tags/e90810b"));
	must_be_true(git_oid_cmp(git_reference_oid(ref_tag), &tag_id) == 0);
	must_be_true(git_oid_cmp(git_reference_oid(ref_tag), &old_tag_id) != 0);

	close_temp_repo(repo);

	git_reference_free(ref_tag);
END_TEST

BEGIN_TEST(write4, "write a lightweight tag to the repository and read it again")
	git_repository *repo;
	git_oid target_id, object_id;
	git_reference *ref_tag;
	git_object *target;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&target_id, tagged_commit);
	must_pass(git_object_lookup(&target, repo, &target_id, GIT_OBJ_COMMIT));

	must_pass(git_tag_create_lightweight(
		&object_id,
		repo,
		"light-tag",
		target,
		0));

	git_object_close(target);

	must_be_true(git_oid_cmp(&object_id, &target_id) == 0);

	must_pass(git_reference_lookup(&ref_tag, repo, "refs/tags/light-tag"));
	must_be_true(git_oid_cmp(git_reference_oid(ref_tag), &target_id) == 0);

	must_pass(git_tag_delete(repo, "light-tag"));

	git_repository_free(repo);

	git_reference_free(ref_tag);
END_TEST

BEGIN_TEST(write5, "Attempt to write a lightweight tag bearing the same name than an already existing tag")
	git_repository *repo;
	git_oid target_id, object_id, existing_object_id;
	git_object *target;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));

	git_oid_fromstr(&target_id, tagged_commit);
	must_pass(git_object_lookup(&target, repo, &target_id, GIT_OBJ_COMMIT));

	must_fail(git_tag_create_lightweight(
		&object_id,
		repo,
		"e90810b",
		target,
		0));

	git_oid_fromstr(&existing_object_id, tag2_id);
	must_be_true(git_oid_cmp(&object_id, &existing_object_id) == 0);

	git_object_close(target);

	git_repository_free(repo);
END_TEST

BEGIN_TEST(delete0, "Delete an already existing tag")
	git_repository *repo;
	git_reference *ref_tag;

	must_pass(open_temp_repo(&repo, REPOSITORY_FOLDER));

	must_pass(git_tag_delete(repo, "e90810b"));

	must_fail(git_reference_lookup(&ref_tag, repo, "refs/tags/e90810b"));

	close_temp_repo(repo);

	git_reference_free(ref_tag);
END_TEST

BEGIN_SUITE(tag)
	ADD_TEST(read0);
	ADD_TEST(read1);
	ADD_TEST(read2);
	ADD_TEST(read3);

	ADD_TEST(write0);
	ADD_TEST(write2);
	ADD_TEST(write3);
	ADD_TEST(write4);
	ADD_TEST(write5);

	ADD_TEST(delete0);

END_SUITE
