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

#include "refs.h"
#include "hash.h"
#include "repository.h"
#include "fileops.h"

#define HASH_SEED 2147483647
#define MAX_NESTING_LEVEL 5

static const int default_table_size = 32;

static struct {
	size_t		size;	/* size in bytes of the object structure */
} git_references_table[] = {
	{sizeof(git_reference_object_id)},	/* 0 = GIT_REF_OBJECT_ID	*/
	{sizeof(git_reference_symbolic)},	/* 1 = GIT_REF_SYMBOLIC		*/
};

static uint32_t reftable_hash(const void *key)
{
	return git__hash(key, strlen((const char *)key), HASH_SEED);
}

static int reftable_haskey(void *reference, const void *key)
{
	git_reference *ref;
	char *name;

	ref = (git_reference *)reference;
	name = (char *)key;

	return strcmp(name, ref->name) == 0;
}

git_reference_database *git_reference_database__alloc()
{
	git_reference_database *ref_database = git__malloc(sizeof(git_reference_database));
	if (!ref_database)
		return NULL;

	memset(ref_database, 0x0, sizeof(git_reference_database));

	ref_database->references = git_hashtable_alloc(
		default_table_size, 
		reftable_hash,
		reftable_haskey);

	if (ref_database->references == NULL) {
		free(ref_database);
		return NULL;
	}

	return ref_database;
}

static void reference__free(git_reference *reference)
{
	assert(reference);

	switch (reference->type) {
	case GIT_REF_SYMBOLIC:
		// The target of the symbolic ref has to be freed by itself.

		/* Fallthrough */

	case GIT_REF_ANY:
	case GIT_REF_OBJECT_ID:
		if (reference->name)
			free(reference->name);

		/* Fallthrough */

	default:
		free(reference);
		break;
	}
}

void git_reference_database__free(git_reference_database *ref_database)
{
	git_hashtable_iterator it;
	git_reference *reference;

	assert(ref_database);

	git_hashtable_iterator_init(ref_database->references, &it);

	while ((reference = (git_reference *)git_hashtable_iterator_next(&it)) != NULL) {
		
		// TODO: Fixme. GITENOTFOUND ? why ?
		
		git_hashtable_remove(ref_database->references, &reference->name);
		reference__free(reference);
	}

	git_hashtable_free(ref_database->references);
	free(ref_database);
}


static int check_refname_validity(const char *name) {
	int error = GIT_SUCCESS;
	
	// TODO : To be implemented
	
	return error;
}

static int reference_newobject(git_reference **reference_out, git_rtype type, const char *name)
{
	git_reference *reference = NULL;

	assert(reference_out && name);

	*reference_out = NULL;

	switch (type) {
	case GIT_REF_OBJECT_ID:
	case GIT_REF_SYMBOLIC:
		break;

	default:
		return GIT_EINVALIDTYPE;
	}

	reference = git__malloc(git_references_table[type].size);

	if (reference == NULL)
		return GIT_ENOMEM;

	memset(reference, 0x0, git_references_table[type].size);

	reference->name = git__malloc(strlen(name) + 1);
	strcpy(reference->name, name);

	reference->type = type;


	*reference_out = reference;
	
	return GIT_SUCCESS;
}

static int symbolic_reference_target_name__parse(char *target_name_out, const char *name, gitfo_buf *buffer) {
	int error = GIT_SUCCESS;
	char *refname_start, *refname_end;
	const char *buffer_end;
	int refname_len;

	refname_start = (char *)buffer->data;
	buffer_end = (const char *)(buffer->data) + buffer->len;

	if (git__prefixcmp(refname_start, GIT_SYMREF))
		return GIT_EREFCORRUPTED;

	refname_start += strlen(GIT_SYMREF);

	/* Skip the potential white spaces */
	while (isspace(refname_start[0]) && refname_start < buffer_end)
		refname_start++;

	refname_end = refname_start;

	/* Seek the end of the target reference name */
	while(!isspace(refname_end[0]) && refname_end < buffer_end)
		refname_end++;

	refname_len = refname_end - refname_start;

	memcpy(target_name_out, refname_start, refname_len);
	target_name_out[refname_len] = 0;

	return error;
}

static int object_id_reference__parse(git_reference **reference_out, const char *name, gitfo_buf *buffer) {
	int error = GIT_SUCCESS;
	git_oid target_oid;
	git_reference *reference;
	char *buffer_out;
	const char *buffer_end;

	buffer_out = (char *)buffer->data;
	buffer_end = (const char *)(buffer->data) + buffer->len;

	/* Is this a valid object id ? */
	if (error = git__parse_oid(&target_oid, &buffer_out, buffer_end, "") < GIT_SUCCESS)
		return error;

	error = reference_newobject(&reference, GIT_REF_OBJECT_ID, name);
	if (error < GIT_SUCCESS)
		return error;

	git_oid_cpy(&((git_reference_object_id *)reference)->id, &target_oid);

	*reference_out = reference;

	return error;
}

static int read_loose_reference(gitfo_buf *file_content, const char *name, const char *path_repository)
{
	int error = GIT_SUCCESS;
	char ref_path[GIT_PATH_MAX];

	/* Determine the full path of the ref */
	strcpy(ref_path, path_repository);
	strcat(ref_path, name);

	/* Does it even exist ? */
	if (gitfo_exists(ref_path) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	/* A ref can not be a directory */
	if (!gitfo_isdir(ref_path))
		return GIT_EINVALIDREFNAME;

	error = gitfo_read_file(file_content, ref_path);

	return error;
}

int git_reference_lookup(git_reference **reference_out, git_reference_database *ref_database, const char *name, const char *path_repository, int *nesting_level)
{
	int error = GIT_SUCCESS;
	gitfo_buf file_content = GITFO_BUF_INIT;
	git_reference *reference, *target_reference;
	git_reference_symbolic *peeled_reference;
	char target_name[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];

	if (*nesting_level == MAX_NESTING_LEVEL) {
		return GIT_ETOONESTEDSYMREF;
	}

	if ((error = check_refname_validity(name)) < GIT_SUCCESS)
		return error;
	
	/* Has the ref already been parsed ? */
	reference = git_hashtable_lookup(ref_database->references, name);
	if (reference != NULL) {
		*reference_out = reference;
		return GIT_SUCCESS;
	}

	/* Has every available ref already been parsed ? */
	if (ref_database->is_fully_loaded)
		return GIT_ENOTFOUND;

	if (*nesting_level == 0) {
		/* Is the database being populated */
		if (ref_database->is_busy)
			return GIT_EBUSY;

		ref_database->is_busy = 1;
	}

	(*nesting_level)++;

	// TODO : Search in packed-refs

	error = read_loose_reference(&file_content, name, path_repository);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Does this look like a symbolic ref ? */
	if (!git__prefixcmp((const char *)(file_content.data), GIT_SYMREF)) {
		error = symbolic_reference_target_name__parse(target_name, name, &file_content);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = git_reference_lookup(&target_reference, ref_database, target_name, path_repository, nesting_level);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = reference_newobject((git_reference **)&peeled_reference, GIT_REF_SYMBOLIC, name);
		if (error < GIT_SUCCESS)
			goto cleanup;

		peeled_reference->target = target_reference;

		reference = (git_reference *)peeled_reference;

		git_hashtable_insert(ref_database->references, reference->name, reference);
		goto found;
	}

	if (object_id_reference__parse(&reference, name, &file_content) < GIT_SUCCESS) {
		error = GIT_EREFCORRUPTED;
		goto cleanup;
	}

	git_hashtable_insert(ref_database->references, reference->name, reference);

found:
	*reference_out = reference;

cleanup:
	(*nesting_level)--;

	if (*nesting_level == 0)
		ref_database->is_busy = 0;
	
	if (file_content.data)
		gitfo_free_buf(&file_content);

	return error;
}
