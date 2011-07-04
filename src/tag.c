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

#include "common.h"
#include "commit.h"
#include "tag.h"
#include "signature.h"
#include "git2/object.h"
#include "git2/repository.h"
#include "git2/signature.h"

void git_tag__free(git_tag *tag)
{
	git_signature_free(tag->tagger);
	free(tag->message);
	free(tag->tag_name);
	free(tag);
}

const git_oid *git_tag_id(git_tag *c)
{
	return git_object_id((git_object *)c);
}

int git_tag_target(git_object **target, git_tag *t)
{
	assert(t);
	return git_object_lookup(target, t->object.repo, &t->target, t->type);
}

const git_oid *git_tag_target_oid(git_tag *t)
{
	assert(t);
	return &t->target;
}

git_otype git_tag_type(git_tag *t)
{
	assert(t);
	return t->type;
}

const char *git_tag_name(git_tag *t)
{
	assert(t);
	return t->tag_name;
}

const git_signature *git_tag_tagger(git_tag *t)
{
	return t->tagger;
}

const char *git_tag_message(git_tag *t)
{
	assert(t);
	return t->message;
}

static int parse_tag_buffer(git_tag *tag, const char *buffer, const char *buffer_end)
{
	static const char *tag_types[] = {
		NULL, "commit\n", "tree\n", "blob\n", "tag\n"
	};

	unsigned int i, text_len;
	char *search;
	int error;

	if ((error = git__parse_oid(&tag->target, &buffer, buffer_end, "object ")) < 0)
		return git__rethrow(error, "Failed to parse tag. Object field invalid");

	if (buffer + 5 >= buffer_end)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Object too short");

	if (memcmp(buffer, "type ", 5) != 0)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Type field not found");
	buffer += 5;

	tag->type = GIT_OBJ_BAD;

	for (i = 1; i < ARRAY_SIZE(tag_types); ++i) {
		size_t type_length = strlen(tag_types[i]);

		if (buffer + type_length >= buffer_end)
			return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Object too short");

		if (memcmp(buffer, tag_types[i], type_length) == 0) {
			tag->type = i;
			buffer += type_length;
			break;
		}
	}

	if (tag->type == GIT_OBJ_BAD)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Invalid object type");

	if (buffer + 4 >= buffer_end)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Object too short");

	if (memcmp(buffer, "tag ", 4) != 0)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Tag field not found");

	buffer += 4;

	search = memchr(buffer, '\n', buffer_end - buffer);
	if (search == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. Object too short");

	text_len = search - buffer;

	tag->tag_name = git__malloc(text_len + 1);
	if (tag->tag_name == NULL)
		return GIT_ENOMEM;

	memcpy(tag->tag_name, buffer, text_len);
	tag->tag_name[text_len] = '\0';

	buffer = search + 1;

	tag->tagger = git__malloc(sizeof(git_signature));
	if (tag->tagger == NULL)
		return GIT_ENOMEM;

	if ((error = git_signature__parse(tag->tagger, &buffer, buffer_end, "tagger ")) != 0) {
		free(tag->tag_name);
		git_signature_free(tag->tagger);
		return git__rethrow(error, "Failed to parse tag");
	}

	if( *buffer != '\n' )
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse tag. No new line before message");

	text_len = buffer_end - ++buffer;

	tag->message = git__malloc(text_len + 1);
	if (tag->message == NULL)
		return GIT_ENOMEM;

	memcpy(tag->message, buffer, text_len);
	tag->message[text_len] = '\0';

	return GIT_SUCCESS;
}

static int retreive_tag_reference(git_reference **tag_reference_out, char *ref_name_out, git_repository *repo, const char *tag_name)
{
	git_reference *tag_ref;
	int error;

	git_path_join(ref_name_out, GIT_REFS_TAGS_DIR, tag_name);
	error = git_reference_lookup(&tag_ref, repo, ref_name_out);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to retrieve tag reference");

	*tag_reference_out = tag_ref;

	return GIT_SUCCESS;
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
	size_t final_size = 0;
	git_odb_stream *stream;

	const char *type_str;
	char *tagger_str;
	git_reference *new_ref = NULL;

	char ref_name[GIT_REFNAME_MAX];

	int type_str_len, tag_name_len, tagger_str_len, message_len;
	int error, should_update_ref = 0;

	if (git_object_owner(target) != repo)
		return git__throw(GIT_EINVALIDARGS, "The given target does not belong to this repository");

	error = retreive_tag_reference(&new_ref, ref_name, repo, tag_name);

	switch (error) {
		case GIT_SUCCESS:
		case GIT_ENOTFOUND:
			break;

		default:
			return git__rethrow(error, "Failed to create tag");
	}

	/** Ensure the tag name doesn't conflict with an already existing
	 *   reference unless overwriting has explictly been requested **/
	if (new_ref != NULL) {
		if (!allow_ref_overwrite) {
			git_oid_cpy(oid, git_reference_oid(new_ref));
			return git__throw(GIT_EEXISTS, "Tag already exists");
		} else {
			should_update_ref = 1;
		}
	}

	type_str = git_object_type2string(git_object_type(target));
	tagger_str_len = git_signature__write(&tagger_str, "tagger", tagger);

	type_str_len = strlen(type_str);
	tag_name_len = strlen(tag_name);
	message_len = strlen(message);

	final_size += GIT_OID_LINE_LENGTH("object");
	final_size += STRLEN("type ") + type_str_len + 1;
	final_size += STRLEN("tag ") + tag_name_len + 1;
	final_size += tagger_str_len;
	final_size += 1 + message_len;

	if ((error = git_odb_open_wstream(&stream, repo->db, final_size, GIT_OBJ_TAG)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	git__write_oid(stream, "object", git_object_id(target));

	stream->write(stream, "type ", STRLEN("type "));
	stream->write(stream, type_str, type_str_len);

	stream->write(stream, "\ntag ", STRLEN("\ntag "));
	stream->write(stream, tag_name, tag_name_len);
	stream->write(stream, "\n", 1);

	stream->write(stream, tagger_str, tagger_str_len);
	free(tagger_str);

	stream->write(stream, "\n", 1);
	stream->write(stream, message, message_len);


	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	if (!should_update_ref)
		error = git_reference_create_oid(&new_ref, repo, ref_name, oid, 0);
	else
		error = git_reference_set_oid(new_ref, oid);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create tag");
}

int git_tag_create_frombuffer(git_oid *oid, git_repository *repo, const char *buffer, int allow_ref_overwrite)
{
	git_tag tag;
	int error, should_update_ref = 0;
	git_odb_stream *stream;
	git_odb_object *target_obj;

	git_reference *new_ref;
	char ref_name[GIT_REFNAME_MAX];

	assert(oid && buffer);

	memset(&tag, 0, sizeof(tag));

	/* validate the buffer */
	if ((error = parse_tag_buffer(&tag, buffer, buffer + strlen(buffer))) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	/* validate the target */
	if ((error = git_odb_read(&target_obj, repo->db, &tag.target)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	if (tag.type != target_obj->raw.type)
		return git__throw(error, "The type for the given target is invalid");

	git_odb_object_close(target_obj);

	error = retreive_tag_reference(&new_ref, ref_name, repo, tag.tag_name);

	switch (error) {
		case GIT_SUCCESS:
		case GIT_ENOTFOUND:
			break;

		default:
			return git__rethrow(error, "Failed to create tag");
	}

	/** Ensure the tag name doesn't conflict with an already existing
	 *   reference unless overwriting has explictly been requested **/
	if (new_ref != NULL) {
		if (!allow_ref_overwrite) {
			git_oid_cpy(oid, git_reference_oid(new_ref));
			return git__throw(GIT_EEXISTS, "Tag already exists");
		} else {
			should_update_ref = 1;
		}
	}

	/* write the buffer */
	if ((error = git_odb_open_wstream(&stream, repo->db, strlen(buffer), GIT_OBJ_TAG)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	stream->write(stream, buffer, strlen(buffer));

	error = stream->finalize_write(oid, stream);
	stream->free(stream);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create tag");

	if (!should_update_ref)
		error = git_reference_create_oid(&new_ref, repo, ref_name, oid, 0);
	else
		error = git_reference_set_oid(new_ref, oid);

	git_signature_free(tag.tagger);
	free(tag.tag_name);
	free(tag.message);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create tag");
}

int git_tag_delete(git_repository *repo, const char *tag_name)
{
	int error;
	git_reference *tag_ref;
	char ref_name[GIT_REFNAME_MAX];

	error = retreive_tag_reference(&tag_ref, ref_name, repo, tag_name);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to delete tag");

	return git_reference_delete(tag_ref);
}

int git_tag__parse(git_tag *tag, git_odb_object *obj)
{
	assert(tag);
	return parse_tag_buffer(tag, obj->raw.data, (char *)obj->raw.data + obj->raw.len);
}

static int tag_list_cb(const char *tag_name, void *payload)
{
	if (git__prefixcmp(tag_name, GIT_REFS_TAGS_DIR) == 0)
		return git_vector_insert((git_vector *)payload, git__strdup(tag_name));

	return GIT_SUCCESS;
}

int git_tag_list(git_strarray *tag_names, git_repository *repo)
{
	int error;
	git_vector taglist;

	if (git_vector_init(&taglist, 8, NULL) < GIT_SUCCESS)
		return GIT_ENOMEM;

	error = git_reference_foreach(repo, GIT_REF_OID|GIT_REF_PACKED, &tag_list_cb, (void *)&taglist);
	if (error < GIT_SUCCESS) {
		git_vector_free(&taglist);
		return git__rethrow(error, "Failed to list tags");
	}

	tag_names->strings = (char **)taglist.contents;
	tag_names->count = taglist.length;
	return GIT_SUCCESS;
}
