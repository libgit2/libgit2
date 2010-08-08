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
#include "repository.h"
#include "commit.h"
#include "tag.h"

static const int default_table_size = 32;
static const double max_load_factor = 0.65;

uint32_t git_repository_object_hash(const void *key)
{
	uint32_t r;
	git_oid *id;

	id = (git_oid *)key;
	memcpy(&r, id->id, sizeof(r));
	return r;
}

int git_repository_object_haskey(void *object, const void *key)
{
	git_repository_object *obj;
	git_oid *oid;

	obj = (git_repository_object *)object;
	oid = (git_oid *)key;

	return (git_oid_cmp(oid, &obj->id) == 0);
}

git_repository *git_repository_alloc(git_odb *odb)
{
	git_repository *repo = git__malloc(sizeof(git_repository));
	if (!repo)
		return NULL;

	memset(repo, 0x0, sizeof(git_repository));

	repo->objects = git_hashtable_alloc(
			default_table_size, 
			git_repository_object_hash,
			git_repository_object_haskey);

	if (repo->objects == NULL) {
		free(repo);
		return NULL;
	}

	repo->db = odb; /* TODO: create ODB manually! */

	return repo;
}

void git_repository_free(git_repository *repo)
{
	git_hashtable_iterator it;
	git_repository_object *object;

	git_hashtable_iterator_init(repo->objects, &it);

	while ((object = (git_repository_object *)
				git_hashtable_iterator_next(&it)) != NULL) {
	
		switch (object->type) {
		case GIT_OBJ_COMMIT:
			git_commit__free((git_commit *)object);
			break;

		case GIT_OBJ_TREE:
			git_tree__free((git_tree *)object);
			break;

		case GIT_OBJ_TAG:
			git_tag__free((git_tag *)object);
			break;

		default:
			free(object);
			break;
		}
	}

	git_hashtable_free(repo->objects);
	/* TODO: free odb */
	free(repo);
}

git_repository_object *git_repository_lookup(git_repository *repo, const git_oid *id, git_otype type)
{
	git_repository_object *object = NULL;
	git_obj obj_file;

	assert(repo);

	object = git_hashtable_lookup(repo->objects, id);
	if (object != NULL)
		return object;

	if (git_odb_read(&obj_file, repo->db, id) < 0 ||
		(type != GIT_OBJ_ANY && type != obj_file.type))
		return NULL;

	object = git__malloc(sizeof(git_commit));

	if (object == NULL)
		return NULL;

	memset(object, 0x0, sizeof(git_commit));

	/* Initialize parent object */
	git_oid_cpy(&object->id, id);
	object->repo = repo;
	object->type = obj_file.type;

	git_hashtable_insert(repo->objects, &object->id, object);
	git_obj_close(&obj_file);

	return object;
}
