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


static int check_refname(const char *name) 
{
	/*
	 * TODO: To be implemented
	 * Check if the given name is a valid name
	 * for a reference
	 */
	
	return name ? GIT_SUCCESS : GIT_ERROR;
}

static void reference_free(git_reference *reference)
{
	if (reference == NULL)
		return;

	if (reference->name)
		free(reference->name);

	if (reference->type == GIT_REF_SYMBOLIC)
		free(reference->target.ref);

	free(reference);
}

int git_reference_new(git_reference **ref_out, git_repository *repo)
{
	git_reference *reference = NULL;

	assert(ref_out && repo);

	reference = git__malloc(sizeof(git_reference));
	if (reference == NULL)
		return GIT_ENOMEM;

	memset(reference, 0x0, sizeof(git_reference));
	reference->type = GIT_REF_INVALID;
	reference->owner = repo;

	*ref_out = reference;
	return GIT_SUCCESS;
}

static int parse_sym_ref(git_reference *ref, gitfo_buf *file_content)
{
	const unsigned int header_len = strlen(GIT_SYMREF);
	const char *refname_start;
	char *eol;

	refname_start = (const char *)file_content->data;

	if (file_content->len < (header_len + 1))
		return GIT_EREFCORRUPTED;

	/* 
	 * Assume we have already checked for the header
	 * before calling this function 
	 */

	refname_start += header_len;

	ref->target.ref = git__strdup(refname_start);
	if (ref->target.ref == NULL)
		return GIT_ENOMEM;

	/* remove newline at the end of file */
	eol = strchr(ref->target.ref, '\n');
	if (eol == NULL)
		return GIT_EREFCORRUPTED;

	*eol = '\0';
	if (eol[-1] == '\r')
		eol[-1] = '\0';

	ref->type = GIT_REF_SYMBOLIC;

	return GIT_SUCCESS;
}

static int parse_oid_ref(git_reference *ref, gitfo_buf *file_content)
{
	char *buffer;

	buffer = (char *)file_content->data;

	/* File format: 40 chars (OID) + newline */
	if (file_content->len < GIT_OID_HEXSZ + 1)
		return GIT_EREFCORRUPTED;

	if (git_oid_mkstr(&ref->target.oid, buffer) < GIT_SUCCESS)
		return GIT_EREFCORRUPTED;

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return GIT_EREFCORRUPTED;

	ref->type = GIT_REF_OID;
	return GIT_SUCCESS;
}

static int read_loose_ref(gitfo_buf *file_content, const char *name, const char *repo_path)
{
	int error = GIT_SUCCESS;
	char ref_path[GIT_PATH_MAX];

	/* Determine the full path of the ref */
	strcpy(ref_path, repo_path);
	strcat(ref_path, name);

	/* Does it even exist ? */
	if (gitfo_exists(ref_path) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	/* A ref can not be a directory */
	if (!gitfo_isdir(ref_path))
		return GIT_ENOTFOUND;

	if (file_content != NULL)
		error = gitfo_read_file(file_content, ref_path);

	return error;
}

static int lookup_loose_ref(
		git_reference **ref_out, 
		git_repository *repo, 
		const char *name)
{
	int error = GIT_SUCCESS;
	gitfo_buf ref_file = GITFO_BUF_INIT;
	git_reference *ref = NULL;

	*ref_out = NULL;

	error = read_loose_ref(&ref_file, name, repo->path_repository);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_reference_new(&ref, repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	ref->name = git__strdup(name);
	if (ref->name == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	if (git__prefixcmp((const char *)(ref_file.data), GIT_SYMREF) == 0)
		error = parse_sym_ref(ref, &ref_file);
	else
		error = parse_oid_ref(ref, &ref_file);

	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_hashtable_insert(repo->references.cache, ref->name, ref);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*ref_out = ref;
	return GIT_SUCCESS;

cleanup:
	gitfo_free_buf(&ref_file);
	reference_free(ref);
	return error;
}


static int read_packed_refs(gitfo_buf *packfile, const char *repo_path)
{
	char ref_path[GIT_PATH_MAX];

	/* Determine the full path of the file */
	strcpy(ref_path, repo_path);
	strcat(ref_path, GIT_PACKEDREFS_FILE);

	/* Does it even exist ? */
	if (gitfo_exists(ref_path) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	return gitfo_read_file(packfile, ref_path);
}

static int parse_packed_line_peel(
		git_reference **ref_out,
		const git_reference *tag_ref, 
		const char **buffer_out, 
		const char *buffer_end)
{
	git_oid oid;
	const char *buffer = *buffer_out + 1;

	assert(buffer[-1] == '^');

	/* Ensure it's not the first entry of the file */
	if (tag_ref == NULL)
		return GIT_EPACKEDREFSCORRUPTED;

	/* Ensure reference is a tag */
	if (git__prefixcmp(tag_ref->name, GIT_REFS_TAGS_DIR) != 0)
		return GIT_EPACKEDREFSCORRUPTED;

	if (buffer + GIT_OID_HEXSZ >= buffer_end)
		return GIT_EPACKEDREFSCORRUPTED;

	/* Is this a valid object id? */
	if (git_oid_mkstr(&oid, buffer) < GIT_SUCCESS)
		return GIT_EPACKEDREFSCORRUPTED;

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return GIT_EPACKEDREFSCORRUPTED;

	*buffer_out = buffer + 1;

	/* 
	 * TODO: do we need the packed line?
	 * Right now we don't, so we don't create a new
	 * reference.
	 */

	*ref_out = NULL;
	return GIT_SUCCESS;
}

static int parse_packed_line(
		git_reference **ref_out,
		git_repository *repo,
		const char **buffer_out,
		const char *buffer_end)
{
	git_reference *ref;

	const char *buffer = *buffer_out;
	const char *refname_begin, *refname_end;

	int error = GIT_SUCCESS;
	int refname_len;

	error = git_reference_new(&ref, repo);
	if (error < GIT_SUCCESS)
		goto cleanup;

	refname_begin = (buffer + GIT_OID_HEXSZ + 1);
	if (refname_begin >= buffer_end ||
		refname_begin[-1] != ' ') {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	/* Is this a valid object id? */
	if ((error = git_oid_mkstr(&ref->target.oid, buffer)) < GIT_SUCCESS)
		goto cleanup;

	refname_end = memchr(refname_begin, '\n', buffer_end - refname_begin);
	if (refname_end == NULL) {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	refname_len = refname_end - refname_begin;

	ref->name = git__malloc(refname_len + 1);
	if (ref->name == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	memcpy(ref->name, refname_begin, refname_len);
	ref->name[refname_len] = 0;

	if (ref->name[refname_len - 1] == '\r')
		ref->name[refname_len - 1] = 0;

	ref->type = GIT_REF_OID;
	ref->packed = 1;

	*ref_out = ref;
	*buffer_out = refname_end + 1;

	return GIT_SUCCESS;

cleanup:
	reference_free(ref);
	return error;
}

static int parse_packed_refs(git_refcache *ref_cache, git_repository *repo)
{
	int error = GIT_SUCCESS;
	gitfo_buf packfile = GITFO_BUF_INIT;
	const char *buffer_start, *buffer_end;

	error = read_packed_refs(&packfile, repo->path_repository);
	if (error < GIT_SUCCESS)
		goto cleanup;

	buffer_start = (const char *)packfile.data;
	buffer_end = (const char *)(buffer_start) + packfile.len;

	/* Does the header look like valid? */
	if (git__prefixcmp((const char *)(buffer_start), GIT_PACKEDREFS_HEADER)) {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	/* Let's skip the header */
	buffer_start += strlen(GIT_PACKEDREFS_HEADER);

	if (*buffer_start == '\r')
		buffer_start++;

	if (*buffer_start != '\n')
		return GIT_EPACKEDREFSCORRUPTED;

	buffer_start++;

	while (buffer_start < buffer_end) {

		git_reference *ref = NULL;
		git_reference *ref_tag = NULL;

		error = parse_packed_line(&ref, repo, &buffer_start, buffer_end);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if (buffer_start[0] == '^') {
			error = parse_packed_line_peel(&ref_tag, ref, &buffer_start, buffer_end);
			if (error < GIT_SUCCESS)
				goto cleanup;
		}

		/*
		 * If a loose reference exists with the same name,
		 * we assume that the loose reference is more up-to-date.
		 * We don't need to cache this ref from the packfile.
		 */
		if (read_loose_ref(NULL, ref->name, repo->path_repository) == GIT_SUCCESS) {
			reference_free(ref);
			reference_free(ref_tag);
			continue;
		}

		error = git_hashtable_insert(ref_cache->cache, ref->name, ref); 
		if (error < GIT_SUCCESS) {
			reference_free(ref);
			reference_free(ref_tag);
			goto cleanup;
		}
	}

	ref_cache->pack_loaded = 1;

cleanup:
	gitfo_free_buf(&packfile);
	return error;
}

void git_reference_set_oid(git_reference *ref, const git_oid *id)
{
	if (ref->type == GIT_REF_SYMBOLIC)
		free(ref->target.ref);

	git_oid_cpy(&ref->target.oid, id);
	ref->type = GIT_REF_OID;

	ref->modified = 1;
}

void git_reference_set_target(git_reference *ref, const char *target)
{
	if (ref->type == GIT_REF_SYMBOLIC)
		free(ref->target.ref);

	ref->target.ref = git__strdup(target);
	ref->type = GIT_REF_SYMBOLIC;

	ref->modified = 1;
}

void git_reference_set_name(git_reference *ref, const char *name)
{
	if (ref->name != NULL) {
		git_hashtable_remove(ref->owner->references.cache, ref->name);
		free(ref->name);
	}

	ref->name = git__strdup(name);
	git_hashtable_insert(ref->owner->references.cache, ref->name, ref);

	ref->modified = 1;
}

const git_oid *git_reference_oid(git_reference *ref)
{
	assert(ref);

	if (ref->type != GIT_REF_OID)
		return NULL;

	return &ref->target.oid;
}

const char *git_reference_target(git_reference *ref)
{
	if (ref->type != GIT_REF_SYMBOLIC)
		return NULL;

	return ref->target.ref;
}

git_rtype git_reference_type(git_reference *ref)
{
	assert(ref);
	return ref->type;
}

const char *git_reference_name(git_reference *ref)
{
	assert(ref);
	return ref->name;
}

git_repository *git_reference_owner(git_reference *ref)
{
	assert(ref);
	return ref->owner;
}

int git_reference_resolve(git_reference **resolved_ref, git_reference *ref)
{
	git_repository *repo;
	int error, i;

	assert(resolved_ref && ref);
	*resolved_ref = NULL;
	
	repo = ref->owner;

	for (i = 0; i < MAX_NESTING_LEVEL; ++i) {

		if (ref->type == GIT_REF_OID) {
			*resolved_ref = ref;
			return GIT_SUCCESS;
		}

		if ((error = git_repository_lookup_ref(&ref, repo, ref->target.ref)) < GIT_SUCCESS)
			return error;
	}

	return GIT_ETOONESTEDSYMREF;
}

int git_reference_write(git_reference *ref)
{
	git_filelock lock;
	char ref_path[GIT_PATH_MAX];
	int error, contents_size;
	char *ref_contents = NULL;

	if (ref->type == GIT_REF_INVALID ||
		ref->name == NULL)
		return GIT_EMISSINGOBJDATA;

	if (ref->modified == 0)
		return GIT_SUCCESS;

	if ((error = check_refname(ref->name)) < GIT_SUCCESS)
		return error;

	strcpy(ref_path, ref->owner->path_repository);
	strcat(ref_path, ref->name);

	if ((error = git_filelock_init(&lock, ref_path)) < GIT_SUCCESS)
		goto error_cleanup;

	if ((error = git_filelock_lock(&lock, 0)) < GIT_SUCCESS)
		goto error_cleanup;

	if (ref->type == GIT_REF_OID) {

		contents_size = GIT_OID_HEXSZ + 1;
		ref_contents = git__malloc(contents_size);
		if (ref_contents == NULL) {
			error = GIT_ENOMEM;
			goto error_cleanup;
		}

		git_oid_fmt(ref_contents, &ref->target.oid);
		ref_contents[contents_size - 1] = '\n';

	} else { /* GIT_REF_SYMBOLIC */

		contents_size = strlen(GIT_SYMREF) + strlen(ref->target.ref) + 1;
		ref_contents = git__malloc(contents_size);
		if (ref_contents == NULL) {
			error = GIT_ENOMEM;
			goto error_cleanup;
		}

		strcpy(ref_contents, GIT_SYMREF);
		strcat(ref_contents, ref->target.ref);
		ref_contents[contents_size - 1] = '\n';
	}

	if ((error = git_filelock_write(&lock, ref_contents, contents_size)) < GIT_SUCCESS)
		goto error_cleanup;

	if ((error = git_filelock_commit(&lock)) < GIT_SUCCESS)
		goto error_cleanup;

	ref->modified = 0;

	free(ref_contents);
	return GIT_SUCCESS;

error_cleanup:
	free(ref_contents);
	git_filelock_unlock(&lock);
	return error;
}

int git_repository_lookup_ref(git_reference **ref_out, git_repository *repo, const char *name)
{
	int error;

	assert(ref_out && repo && name);

	*ref_out = NULL;

	error = check_refname(name);
	if (error < GIT_SUCCESS)
		return error;

	/*
	 * First, check if the reference is on the local cache;
	 * references on the cache are assured to be up-to-date
	 */
	*ref_out = git_hashtable_lookup(repo->references.cache, name);
	if (*ref_out != NULL)
		return GIT_SUCCESS;

	/*
	 * Then check if there is a loose file for that reference.
	 * If the file exists, we parse it and store it on the
	 * cache.
	 */
	error = lookup_loose_ref(ref_out, repo, name);

	if (error == GIT_SUCCESS)
		return GIT_SUCCESS;

	if (error != GIT_ENOTFOUND)
		return error;

	/*
	 * Check if we have loaded the packed references.
	 * If the packed references have been loaded, they would be
	 * stored already on the cache: that means that the ref
	 * we are looking for doesn't exist.
	 *
	 * If they haven't been loaded yet, we load the packfile
	 * and check if our reference is inside of it.
	 */
	if (!repo->references.pack_loaded) {

		/* load all the packed references */
		error = parse_packed_refs(&repo->references, repo);
		if (error < GIT_SUCCESS)
			return error;

		/* check the cache again -- hopefully the reference will be there */
		*ref_out = git_hashtable_lookup(repo->references.cache, name);
		if (*ref_out != NULL)
			return GIT_SUCCESS;
	}

	/* The reference doesn't exist anywhere */
	return GIT_ENOTFOUND;
}

int git_repository__refcache_init(git_refcache *refs)
{
	assert(refs);

	refs->cache = git_hashtable_alloc(
		default_table_size, 
		reftable_hash,
		reftable_haskey);

	return refs->cache ? GIT_SUCCESS : GIT_ENOMEM; 
}

void git_repository__refcache_free(git_refcache *refs)
{
	git_hashtable_iterator it;
	git_reference *reference;

	assert(refs);

	git_hashtable_iterator_init(refs->cache, &it);

	while ((reference = (git_reference *)git_hashtable_iterator_next(&it)) != NULL) {
		git_hashtable_remove(refs->cache, reference->name);
		reference_free(reference);
	}

	git_hashtable_free(refs->cache);
}


