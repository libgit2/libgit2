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

static const int default_table_size = 32;

uint32_t reftable_hash(const void *key)
{
	uint32_t r;
	git_hash_ctx *ctx;
	git_oid sha1ed_ref_name;
	const char *ref_name;

	ref_name = (const char *)key;

	ctx = git_hash_new_ctx();
	if (ctx == NULL)
		return GIT_ENOMEM;	// TODO: Fixme. This could be a valid hash.

	git_hash_update(ctx, ref_name, strlen(ref_name));
	git_hash_final(&sha1ed_ref_name, ctx);

	git_hash_free_ctx(ctx);

	memcpy(&r, &sha1ed_ref_name.id, sizeof(r));
	return r;
}

int reftable_haskey(void *reference, const void *key)
{
	git_reference *ref;
	char *name;

	ref = (git_reference *)reference;
	name = (char *)key;

	return (strcmp(name, ref->name) == 0);
}

reference_database *reference_database__alloc() {
	reference_database *ref_database = git__malloc(sizeof(reference_database));
	if (!ref_database)
		return NULL;

	memset(ref_database, 0x0, sizeof(reference_database));

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

void reference__free(git_reference *reference) {
	assert(reference);

	free(reference->name);
	free(reference);
}

void reference_database__free(reference_database *ref_database) {
	git_hashtable_iterator it;
	git_reference *reference;

	assert(ref_database);

	git_hashtable_iterator_init(ref_database->references, &it);

	while ((reference = (git_reference *)git_hashtable_iterator_next(&it)) != NULL) {
		git_hashtable_remove(ref_database->references, &reference->name);
		reference__free(reference);
	}

	git_hashtable_free(ref_database->references);
	free(ref_database);
}
