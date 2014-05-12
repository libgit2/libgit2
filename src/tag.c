/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "commit.h"
#include "tag.h"
#include "signature.h"
#include "message.h"
#include "git2/object.h"
#include "git2/repository.h"
#include "git2/signature.h"
#include "git2/odb_backend.h"

void git_tag__free(void *_tag)
{
	git_tag *tag = _tag;
	git_signature_free(tag->tagger);
	git__free(tag->message);
	git__free(tag->tag_name);
	git__free(tag);
}

int git_tag_target(git_object **target, const git_tag *t)
{
	assert(t);
	return git_object_lookup(target, t->object.repo, &t->target, t->type);
}

const git_oid *git_tag_target_id(const git_tag *t)
{
	assert(t);
	return &t->target;
}

git_otype git_tag_target_type(const git_tag *t)
{
	assert(t);
	return t->type;
}

const char *git_tag_name(const git_tag *t)
{
	assert(t);
	return t->tag_name;
}

const git_signature *git_tag_tagger(const git_tag *t)
{
	return t->tagger;
}

const char *git_tag_message(const git_tag *t)
{
	assert(t);
	return t->message;
}

static int tag_parse(git_tag *tag, const char *buf, const char *buf_end)
{
	int error = 0;
	const char *body = NULL;
	git_object_parse_t parser[] = {
		{ "object", 6, GIT_PARSE_OID, { .id = &tag->target } },
		{ "type", 4, GIT_PARSE_OTYPE, { .otype = &tag->type } },
		{ NULL, 0, GIT_PARSE_MODE_OPTIONAL },
		{ "tag", 3, GIT_PARSE_TO_EOL, { .text = &tag->tag_name } },
		{ "tagger", 6, GIT_PARSE_SIGNATURE, { .sig = &tag->tagger } },
		{ NULL, 0, GIT_PARSE_BODY_OPTIONAL, { .body = &body } },
	};

	error = git_object__parse_lines(GIT_OBJ_TAG, parser, buf, buf_end);

	if (body != NULL && body < buf_end) {
		tag->message = git__strndup(body, buf_end - body);
		GITERR_CHECK_ALLOC(tag->message);
	}

	return error;
}

int git_tag__parse(void *tag, git_odb_object *odb_obj)
{
	const char *buffer = git_odb_object_data(odb_obj);
	const char *buffer_end = buffer + git_odb_object_size(odb_obj);
	return tag_parse(tag, buffer, buffer_end);
}

static int retrieve_tag_reference(
	git_reference **tag_reference_out,
	git_buf *ref_name_out,
	git_repository *repo,
	const char *tag_name)
{
	git_reference *tag_ref;
	int error;

	*tag_reference_out = NULL;

	if (git_buf_joinpath(ref_name_out, GIT_REFS_TAGS_DIR, tag_name) < 0)
		return -1;

	error = git_reference_lookup(&tag_ref, repo, ref_name_out->ptr);
	if (error < 0)
		return error; /* Be it not foundo or corrupted */

	*tag_reference_out = tag_ref;

	return 0;
}

static int retrieve_tag_reference_oid(
	git_oid *oid,
	git_buf *ref_name_out,
	git_repository *repo,
	const char *tag_name)
{
	if (git_buf_joinpath(ref_name_out, GIT_REFS_TAGS_DIR, tag_name) < 0)
		return -1;

	return git_reference_name_to_id(oid, repo, ref_name_out->ptr);
}

static int write_tag_annotation(
	git_oid *oid,
	git_repository *repo,
	const char *tag_name,
	const git_object *target,
	const git_signature *tagger,
	const char *message)
{
	git_buf tag = GIT_BUF_INIT;
	git_odb *odb;

	git_oid__writebuf(&tag, "object ", git_object_id(target));
	git_buf_printf(&tag, "type %s\n", git_object_type2string(git_object_type(target)));
	git_buf_printf(&tag, "tag %s\n", tag_name);
	git_signature__writebuf(&tag, "tagger ", tagger);
	git_buf_putc(&tag, '\n');

	if (git_buf_puts(&tag, message) < 0)
		goto on_error;

	if (git_repository_odb__weakptr(&odb, repo) < 0)
		goto on_error;

	if (git_odb_write(oid, odb, tag.ptr, tag.size, GIT_OBJ_TAG) < 0)
		goto on_error;

	git_buf_free(&tag);
	return 0;

on_error:
	git_buf_free(&tag);
	giterr_set(GITERR_OBJECT, "Failed to create tag annotation.");
	return -1;
}

static int git_tag_create__internal(
	git_oid *oid,
	git_repository *repo,
	const char *tag_name,
	const git_object *target,
	const git_signature *tagger,
	const char *message,
	int allow_ref_overwrite,
	int create_tag_annotation)
{
	git_reference *new_ref = NULL;
	git_buf ref_name = GIT_BUF_INIT;

	int error;

	assert(repo && tag_name && target);
	assert(!create_tag_annotation || (tagger && message));

	if (git_object_owner(target) != repo) {
		giterr_set(GITERR_INVALID, "The given target does not belong to this repository");
		return -1;
	}

	error = retrieve_tag_reference_oid(oid, &ref_name, repo, tag_name);
	if (error < 0 && error != GIT_ENOTFOUND)
		goto cleanup;

	/** Ensure the tag name doesn't conflict with an already existing
	 *	reference unless overwriting has explictly been requested **/
	if (error == 0 && !allow_ref_overwrite) {
		git_buf_free(&ref_name);
		giterr_set(GITERR_TAG, "Tag already exists");
		return GIT_EEXISTS;
	}

	if (create_tag_annotation) {
		if (write_tag_annotation(oid, repo, tag_name, target, tagger, message) < 0)
			return -1;
	} else
		git_oid_cpy(oid, git_object_id(target));

	error = git_reference_create(&new_ref, repo, ref_name.ptr, oid, allow_ref_overwrite, NULL, NULL);

cleanup:
	git_reference_free(new_ref);
	git_buf_free(&ref_name);
	return error;
}

int git_tag_create(
	git_oid *oid,
	git_repository *repo,
	const char *tag_name,
	const git_object *target,
	const git_signature *tagger,
	const char *message,
	int allow_ref_overwrite)
{
	return git_tag_create__internal(oid, repo, tag_name, target, tagger, message, allow_ref_overwrite, 1);
}

int git_tag_annotation_create(
	git_oid *oid,
	git_repository *repo,
	const char *tag_name,
	const git_object *target,
	const git_signature *tagger,
	const char *message)
{
	assert(oid && repo && tag_name && target && tagger && message);

	return write_tag_annotation(oid, repo, tag_name, target, tagger, message);
}

int git_tag_create_lightweight(
	git_oid *oid,
	git_repository *repo,
	const char *tag_name,
	const git_object *target,
	int allow_ref_overwrite)
{
	return git_tag_create__internal(oid, repo, tag_name, target, NULL, NULL, allow_ref_overwrite, 0);
}

int git_tag_create_frombuffer(git_oid *oid, git_repository *repo, const char *buffer, int allow_ref_overwrite)
{
	git_tag tag;
	int error;
	git_odb *odb;
	git_odb_stream *stream;
	git_odb_object *target_obj = NULL;
	git_buf ref_name = GIT_BUF_INIT;
	size_t buflen;

	assert(oid && buffer);

	memset(&tag, 0, sizeof(tag));
	buflen = strlen(buffer);

	/* validate the buffer */
	if ((error = tag_parse(&tag, buffer, buffer + buflen)) < 0)
		goto cleanup;

	/* validate the target */
	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0 ||
		(error = git_odb_read(&target_obj, odb, &tag.target)) < 0)
		goto cleanup;

	if (tag.type != target_obj->cached.type) {
		giterr_set(GITERR_TAG, "The type for the given target is invalid");
		error = -1;
		goto cleanup;
	}

	error = retrieve_tag_reference_oid(oid, &ref_name, repo, tag.tag_name);
	if (error < 0 && error != GIT_ENOTFOUND)
		goto cleanup;

	/** Ensure the tag name doesn't conflict with an already existing
	 *	reference unless overwriting has explictly been requested **/
	if (!error && !allow_ref_overwrite) {
		giterr_set(GITERR_TAG, "Tag already exists");
		error = GIT_EEXISTS;
		goto cleanup;
	}

	/* write the buffer */
	if (!(error = git_odb_open_wstream(&stream, odb, buflen, GIT_OBJ_TAG))) {

		if (!(error = git_odb_stream_write(stream, buffer, buflen)))
			error = git_odb_stream_finalize_write(oid, stream);

		git_odb_stream_free(stream);
	}

	/* update the reference */
	if (!error) {
		git_reference *new_ref = NULL;

		error = git_reference_create(
			&new_ref, repo, ref_name.ptr, oid,
			allow_ref_overwrite, NULL, NULL);

		git_reference_free(new_ref);
	}

cleanup:
	git_signature_free(tag.tagger);
	git__free(tag.tag_name);
	git__free(tag.message);
	git_odb_object_free(target_obj);
	git_buf_free(&ref_name);

	return error;
}

int git_tag_delete(git_repository *repo, const char *tag_name)
{
	git_reference *tag_ref;
	git_buf ref_name = GIT_BUF_INIT;
	int error;

	error = retrieve_tag_reference(&tag_ref, &ref_name, repo, tag_name);

	git_buf_free(&ref_name);

	if (!error)
		error = git_reference_delete(tag_ref);

	git_reference_free(tag_ref);

	return error;
}

typedef struct {
	git_repository *repo;
	git_tag_foreach_cb cb;
	void *cb_data;
} tag_cb_data;

static int tags_cb(const char *ref, void *data)
{
	int error;
	git_oid oid;
	tag_cb_data *d = (tag_cb_data *)data;

	if (git__prefixcmp(ref, GIT_REFS_TAGS_DIR) != 0)
		return 0; /* no tag */

	if (!(error = git_reference_name_to_id(&oid, d->repo, ref))) {
		if ((error = d->cb(ref, &oid, d->cb_data)) != 0)
			giterr_set_after_callback_function(error, "git_tag_foreach");
	}

	return error;
}

int git_tag_foreach(git_repository *repo, git_tag_foreach_cb cb, void *cb_data)
{
	tag_cb_data data;

	assert(repo && cb);

	data.cb = cb;
	data.cb_data = cb_data;
	data.repo = repo;

	return git_reference_foreach_name(repo, &tags_cb, &data);
}

typedef struct {
	git_vector *taglist;
	const char *pattern;
} tag_filter_data;

#define GIT_REFS_TAGS_DIR_LEN strlen(GIT_REFS_TAGS_DIR)

static int tag_list_cb(const char *tag_name, git_oid *oid, void *data)
{
	tag_filter_data *filter = (tag_filter_data *)data;
	GIT_UNUSED(oid);

	if (!*filter->pattern ||
		p_fnmatch(filter->pattern, tag_name + GIT_REFS_TAGS_DIR_LEN, 0) == 0)
	{
		char *matched = git__strdup(tag_name + GIT_REFS_TAGS_DIR_LEN);
		GITERR_CHECK_ALLOC(matched);

		return git_vector_insert(filter->taglist, matched);
	}

	return 0;
}

int git_tag_list_match(git_strarray *tag_names, const char *pattern, git_repository *repo)
{
	int error;
	tag_filter_data filter;
	git_vector taglist;

	assert(tag_names && repo && pattern);

	if ((error = git_vector_init(&taglist, 8, NULL)) < 0)
		return error;

	filter.taglist = &taglist;
	filter.pattern = pattern;

	error = git_tag_foreach(repo, &tag_list_cb, (void *)&filter);

	if (error < 0)
		git_vector_free(&taglist);

	tag_names->strings =
		(char **)git_vector_detach(&tag_names->count, NULL, &taglist);

	return 0;
}

int git_tag_list(git_strarray *tag_names, git_repository *repo)
{
	return git_tag_list_match(tag_names, "", repo);
}

int git_tag_peel(git_object **tag_target, const git_tag *tag)
{
	return git_object_peel(tag_target, (const git_object *)tag, GIT_OBJ_ANY);
}
