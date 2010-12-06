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
#include "person.h"
#include "revwalk.h"
#include "git2/object.h"
#include "git2/repository.h"

void git_tag__free(git_tag *tag)
{
	git_person__free(tag->tagger);
	free(tag->message);
	free(tag->tag_name);
	free(tag);
}

const git_oid *git_tag_id(git_tag *c)
{
	return git_object_id((git_object *)c);
}

const git_object *git_tag_target(git_tag *t)
{
	assert(t);
	return t->target;
}

void git_tag_set_target(git_tag *tag, git_object *target)
{
	assert(tag && target);

	tag->object.modified = 1;
	tag->target = target;
	tag->type = git_object_type(target);
}

git_otype git_tag_type(git_tag *t)
{
	assert(t);
	return t->type;
}

void git_tag_set_type(git_tag *tag, git_otype type)
{
	assert(tag);

	tag->object.modified = 1;
	tag->type = type;
}

const char *git_tag_name(git_tag *t)
{
	assert(t);
	return t->tag_name;
}

void git_tag_set_name(git_tag *tag, const char *name)
{
	assert(tag && name);

	/* TODO: sanity check? no newlines in message */
	tag->object.modified = 1;

	if (tag->tag_name)
		free(tag->tag_name);

	tag->tag_name = git__strdup(name);
}

const git_person *git_tag_tagger(git_tag *t)
{
	return t->tagger;
}

void git_tag_set_tagger(git_tag *tag, const char *name, const char *email, time_t time)
{
	assert(tag && name && email);
	tag->object.modified = 1;

	git_person__free(tag->tagger);
	tag->tagger = git_person__new(name, email, time);
}

const char *git_tag_message(git_tag *t)
{
	assert(t);
	return t->message;
}

void git_tag_set_message(git_tag *tag, const char *message)
{
	assert(tag && message);

	tag->object.modified = 1;

	if (tag->message)
		free(tag->message);

	tag->message = git__strdup(message);
}

static int parse_tag_buffer(git_tag *tag, char *buffer, const char *buffer_end)
{
	static const char *tag_types[] = {
		NULL, "commit\n", "tree\n", "blob\n", "tag\n"
	};

	git_oid target_oid;
	unsigned int i, text_len;
	char *search;
	int error;

	if ((error = git__parse_oid(&target_oid, &buffer, buffer_end, "object ")) < 0)
		return error;

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

	error = git_repository_lookup(&tag->target, tag->object.repo, &target_oid, tag->type);
	if (error < 0)
		return error;

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
		git_person__free(tag->tagger);

	tag->tagger = git__malloc(sizeof(git_person));

	if ((error = git_person__parse(tag->tagger, &buffer, buffer_end, "tagger ")) != 0)
		return error;

	text_len = buffer_end - ++buffer;

	if (tag->message != NULL)
		free(tag->message);

	tag->message = git__malloc(text_len + 1);
	memcpy(tag->message, buffer, text_len);
	tag->message[text_len] = '\0';

	return GIT_SUCCESS;
}

int git_tag__writeback(git_tag *tag, git_odb_source *src)
{
	if (tag->target == NULL || tag->tag_name == NULL || tag->tagger == NULL)
		return GIT_EMISSINGOBJDATA;

	git__write_oid(src, "object", git_object_id(tag->target));
	git__source_printf(src, "type %s\n", git_object_type2string(tag->type));
	git__source_printf(src, "tag %s\n", tag->tag_name);
	git_person__write(src, "tagger", tag->tagger);

	if (tag->message != NULL)
		git__source_printf(src, "\n%s", tag->message);

	return GIT_SUCCESS;
}


int git_tag__parse(git_tag *tag)
{
	assert(tag && tag->object.source.open);
	return parse_tag_buffer(tag, tag->object.source.raw.data, (char *)tag->object.source.raw.data + tag->object.source.raw.len);
}

