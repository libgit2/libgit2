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
#include "revwalk.h"
#include "git/odb.h"


void git_tag__free(git_tag *tag)
{
	free(tag->message);
	free(tag->tag_name);
	free(tag->tagger);
	free(tag);
}

const git_oid *git_tag_id(git_tag *t)
{
	return &t->object.id;
}

const git_revpool_object *git_tag_target(git_tag *t)
{
	if (t->target)
		return t->target;

	git_tag__parse(t);
	return t->target;
}

git_otype git_tag_type(git_tag *t)
{
	if (t->type)
		return t->type;

	git_tag__parse(t);
	return t->type;
}


const char *git_tag_name(git_tag *t)
{
	if (t->tag_name)
		return t->tag_name;

	git_tag__parse(t);
	return t->tag_name;
}

const git_person *git_tag_tagger(git_tag *t)
{
	if (t->tagger)
		return t->tagger;

	git_tag__parse(t);
	return t->tagger;
}

const char *git_tag_message(git_tag *t)
{
	if (t->message)
		return t->message;

	git_tag__parse(t);
	return t->message;
}

static int parse_tag_buffer(git_tag *tag, char *buffer, const char *buffer_end)
{
	static const char *tag_types[] = {
		NULL, "commit\n", "tree\n", "blob\n", "tag\n"
	};

	git_oid target_oid;
	unsigned int i, text_len;
	char *search;

	if (git__parse_oid(&target_oid, &buffer, buffer_end, "object ") < 0)
		return GIT_EOBJCORRUPTED;

	if (buffer + 5 >= buffer_end)
		return GIT_EOBJCORRUPTED;

	if (memcmp(buffer, "type ", 5) != 0)
		return GIT_EOBJCORRUPTED;
	buffer += 5;

	tag->type = GIT_OBJ_BAD;

	for (i = 1; i < ARRAY_SIZE(tag_types); ++i) {
		size_t type_length = strlen(tag_types[i]);

		if (buffer + type_length >= buffer_end)
			return GIT_EOBJCORRUPTED;

		if (memcmp(buffer, tag_types[i], type_length) == 0) {
			tag->type = i;
			buffer += type_length;
			break;
		}
	}

	if (tag->type == GIT_OBJ_BAD)
		return GIT_EOBJCORRUPTED;

	/* Lookup the tagged object once we know its type */
	tag->target = 
		git_repository_lookup(tag->object.repo, &target_oid, tag->type);

	/* FIXME: is the tag file really corrupted if the tagged object
	 * cannot be found on the database? */
	/* if (tag->target == NULL)
		return GIT_EOBJCORRUPTED; */

	if (buffer + 4 >= buffer_end)
		return GIT_EOBJCORRUPTED;

	if (memcmp(buffer, "tag ", 4) != 0)
		return GIT_EOBJCORRUPTED;
	buffer += 4;

	search = memchr(buffer, '\n', buffer_end - buffer);
	if (search == NULL)
		return GIT_EOBJCORRUPTED;

	text_len = search - buffer;

	if (tag->tag_name != NULL)
		free(tag->tag_name);

	tag->tag_name = git__malloc(text_len + 1);
	memcpy(tag->tag_name, buffer, text_len);
	tag->tag_name[text_len] = '\0';

	buffer = search + 1;

	if (tag->tagger != NULL)
		free(tag->tagger);

	tag->tagger = git__malloc(sizeof(git_person));

	if (git__parse_person(tag->tagger, &buffer, buffer_end, "tagger ") != 0)
		return GIT_EOBJCORRUPTED;

	text_len = buffer_end - buffer;

	if (tag->message != NULL)
		free(tag->message);

	tag->message = git__malloc(text_len + 1);
	memcpy(tag->message, buffer, text_len);
	tag->message[text_len] = '\0';

	return 0;
}

int git_tag__parse(git_tag *tag)
{
	int error = 0;
	git_obj odb_object;

	error = git_odb_read(&odb_object, tag->object.pool->db, &tag->object.id);
	if (error < 0)
		return error;

	if (odb_object.type != GIT_OBJ_TAG) {
		error = GIT_EOBJTYPE;
		goto cleanup;
	}

	error = parse_tag_buffer(tag, odb_object.data, odb_object.data + odb_object.len);

cleanup:
	git_obj_close(&odb_object);
	return error;
}

git_tag *git_tag_lookup(git_revpool *pool, const git_oid *id)
{
	git_tag *tag = NULL;

	if (pool == NULL)
		return NULL;

	tag = (git_tag *)git_revpool_table_lookup(pool->objects, id);
	if (tag != NULL)
		return tag;

	tag = git__malloc(sizeof(git_tag));

	if (tag == NULL)
		return NULL;

	memset(tag, 0x0, sizeof(git_tag));

	/* Initialize parent object */
	git_oid_cpy(&tag->object.id, id);
	tag->object.pool = pool;
	tag->object.type = GIT_OBJ_TAG;

	git_revpool_table_insert(pool->objects, (git_revpool_object *)tag);

	return tag;
}
