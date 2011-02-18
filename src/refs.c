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

#define MAX_NESTING_LEVEL 5

typedef struct {
	git_reference ref;
	git_oid oid;
	git_oid peel_target;
} reference_oid;

typedef struct {
	git_reference ref;
	char *target;
} reference_symbolic;


static int reference_write(git_reference *ref);
static int normalize_name(char *buffer_out, const char *name, int is_oid_ref);

static const int default_table_size = 32;

static uint32_t reftable_hash(const void *key, int hash_id)
{
	static uint32_t hash_seeds[GIT_HASHTABLE_HASHES] = {
		2147483647,
		0x5d20bb23,
		0x7daaab3c 
	};

	return git__hash(key, strlen((const char *)key), hash_seeds[hash_id]);
}

static void reference_free(git_reference *reference)
{
	if (reference == NULL)
		return;

	if (reference->name)
		free(reference->name);

	if (reference->type == GIT_REF_SYMBOLIC)
		free(((reference_symbolic *)reference)->target);

	free(reference);
}

static int reference_create(git_reference **ref_out, git_repository *repo, const char *name, git_rtype type) {
	char normalized[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];
	int error = GIT_SUCCESS, size;
	git_reference *reference = NULL;

	assert(ref_out && repo && name);

	if (type == GIT_REF_SYMBOLIC)
		size = sizeof(reference_symbolic);
	else if (type == GIT_REF_OID)
		size = sizeof(reference_oid);
	else
		return GIT_EINVALIDREFSTATE;

	reference = git__malloc(size);
	if (reference == NULL)
		return GIT_ENOMEM;

	memset(reference, 0x0, size);
	reference->owner = repo;
	reference->type = type;

	error = normalize_name(normalized, name, (type & GIT_REF_OID));
	if (error < GIT_SUCCESS)
		goto cleanup;

	reference->name = git__strdup(normalized);
	if (reference->name == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	*ref_out = reference;

	return error;

cleanup:
	reference_free(reference);
	return error;
}

int git_reference_create_symbolic(git_reference **ref_out, git_repository *repo, const char *name, const char *target)
{
	char normalized[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];
	int error = GIT_SUCCESS;
	git_reference *ref = NULL;

	error = reference_create(&ref, repo, name, GIT_REF_SYMBOLIC);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* The target can aither be the name of an object id reference or the name of another symbolic reference */
	error = normalize_name(normalized, target, 0);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* set the target; this will write the reference on disk */
	error = git_reference_set_target(ref, normalized);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_hashtable_insert(repo->references.loose_refs, ref->name, ref);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*ref_out = ref;

	return error;

cleanup:
	reference_free(ref);
	return error;
}

int git_reference_create_oid(git_reference **ref_out, git_repository *repo, const char *name, const git_oid *id)
{
	int error = GIT_SUCCESS;
	git_reference *ref = NULL;

	error = reference_create(&ref, repo, name, GIT_REF_OID);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* set the oid; this will write the reference on disk */
	error = git_reference_set_oid(ref, id);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_hashtable_insert(repo->references.loose_refs, ref->name, ref);
	if (error < GIT_SUCCESS)
		goto cleanup;

	*ref_out = ref;

	return error;

cleanup:
	reference_free(ref);
	return error;
}

static int parse_sym_ref(git_reference *ref, gitfo_buf *file_content)
{
	const unsigned int header_len = strlen(GIT_SYMREF);
	const char *refname_start;
	char *eol;
	reference_symbolic *ref_sym;

	refname_start = (const char *)file_content->data;
	ref_sym = (reference_symbolic *)ref;

	if (file_content->len < (header_len + 1))
		return GIT_EREFCORRUPTED;

	/* 
	 * Assume we have already checked for the header
	 * before calling this function 
	 */

	refname_start += header_len;

	ref_sym->target = git__strdup(refname_start);
	if (ref_sym->target == NULL)
		return GIT_ENOMEM;

	/* remove newline at the end of file */
	eol = strchr(ref_sym->target, '\n');
	if (eol == NULL)
		return GIT_EREFCORRUPTED;

	*eol = '\0';
	if (eol[-1] == '\r')
		eol[-1] = '\0';

	return GIT_SUCCESS;
}

static int parse_oid_ref(git_reference *ref, gitfo_buf *file_content)
{
	reference_oid *ref_oid;
	char *buffer;

	buffer = (char *)file_content->data;
	ref_oid = (reference_oid *)ref;

	/* File format: 40 chars (OID) + newline */
	if (file_content->len < GIT_OID_HEXSZ + 1)
		return GIT_EREFCORRUPTED;

	if (git_oid_mkstr(&ref_oid->oid, buffer) < GIT_SUCCESS)
		return GIT_EREFCORRUPTED;

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return GIT_EREFCORRUPTED;

	return GIT_SUCCESS;
}

static int read_loose_ref(gitfo_buf *file_content, const char *name, const char *repo_path)
{
	int error = GIT_SUCCESS;
	char ref_path[GIT_PATH_MAX];

	/* Determine the full path of the ref */
	git__joinpath(ref_path, repo_path, name);

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

	if (git__prefixcmp((const char *)(ref_file.data), GIT_SYMREF) == 0) {
		error = reference_create(&ref, repo, name, GIT_REF_SYMBOLIC);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = parse_sym_ref(ref, &ref_file);
	} else {
		error = reference_create(&ref, repo, name, GIT_REF_OID);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = parse_oid_ref(ref, &ref_file);
	}

	if (error < GIT_SUCCESS)
		goto cleanup;

	error = git_hashtable_insert(repo->references.loose_refs, ref->name, ref);
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
	git__joinpath(ref_path, repo_path, GIT_PACKEDREFS_FILE);

	/* Does it even exist ? */
	if (gitfo_exists(ref_path) < GIT_SUCCESS)
		return GIT_ENOTFOUND;

	return gitfo_read_file(packfile, ref_path);
}

static int parse_packed_line_peel(
		reference_oid *tag_ref,
		const char **buffer_out, 
		const char *buffer_end)
{
	const char *buffer = *buffer_out + 1;

	assert(buffer[-1] == '^');

	/* Ensure it's not the first entry of the file */
	if (tag_ref == NULL)
		return GIT_EPACKEDREFSCORRUPTED;

	/* Ensure reference is a tag */
	if (git__prefixcmp(tag_ref->ref.name, GIT_REFS_TAGS_DIR) != 0)
		return GIT_EPACKEDREFSCORRUPTED;

	if (buffer + GIT_OID_HEXSZ >= buffer_end)
		return GIT_EPACKEDREFSCORRUPTED;

	/* Is this a valid object id? */
	if (git_oid_mkstr(&tag_ref->peel_target, buffer) < GIT_SUCCESS)
		return GIT_EPACKEDREFSCORRUPTED;

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return GIT_EPACKEDREFSCORRUPTED;

	*buffer_out = buffer + 1;
	tag_ref->ref.type |= GIT_REF_HAS_PEEL;

	return GIT_SUCCESS;
}

static int parse_packed_line(
		reference_oid **ref_out,
		git_repository *repo,
		const char **buffer_out,
		const char *buffer_end)
{
	reference_oid *ref;

	const char *buffer = *buffer_out;
	const char *refname_begin, *refname_end;

	int error = GIT_SUCCESS;
	int refname_len;
	char refname[MAX_GITDIR_TREE_STRUCTURE_PATH_LENGTH];
	git_oid id;

	refname_begin = (buffer + GIT_OID_HEXSZ + 1);
	if (refname_begin >= buffer_end ||
		refname_begin[-1] != ' ') {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	/* Is this a valid object id? */
	if ((error = git_oid_mkstr(&id, buffer)) < GIT_SUCCESS)
		goto cleanup;

	refname_end = memchr(refname_begin, '\n', buffer_end - refname_begin);
	if (refname_end == NULL) {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	refname_len = refname_end - refname_begin;

	memcpy(refname, refname_begin, refname_len);
	refname[refname_len] = 0;

	if (refname[refname_len - 1] == '\r')
		refname[refname_len - 1] = 0;

	error = reference_create((git_reference **)&ref, repo, refname, GIT_REF_OID);
	if (error < GIT_SUCCESS)
		goto cleanup;

	git_oid_cpy(&ref->oid, &id);
	ref->ref.type |= GIT_REF_PACKED;

	*ref_out = ref;
	*buffer_out = refname_end + 1;

	return GIT_SUCCESS;

cleanup:
	reference_free((git_reference *)ref);
	return error;
}

static int load_packed_refs(git_refcache *ref_cache, git_repository *repo)
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

		reference_oid *ref = NULL;

		error = parse_packed_line(&ref, repo, &buffer_start, buffer_end);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if (buffer_start[0] == '^') {
			error = parse_packed_line_peel(ref, &buffer_start, buffer_end);
			if (error < GIT_SUCCESS)
				goto cleanup;
		}

		error = git_hashtable_insert(ref_cache->packed_refs, ref->ref.name, ref);
		if (error < GIT_SUCCESS) {
			reference_free((git_reference *)ref);
			goto cleanup;
		}
	}

	ref_cache->pack_loaded = 1;

cleanup:
	gitfo_free_buf(&packfile);
	return error;
}

int git_reference_set_oid(git_reference *ref, const git_oid *id)
{
	reference_oid *ref_oid;

	if ((ref->type & GIT_REF_OID) == 0)
		return GIT_EINVALIDREFSTATE;

	ref_oid = (reference_oid *)ref;
	git_oid_cpy(&ref_oid->oid, id);

	ref->type &= ~GIT_REF_HAS_PEEL;
	/* TODO: set new peel target */

	return reference_write(ref);
}

int git_reference_set_target(git_reference *ref, const char *target)
{
	reference_symbolic *ref_sym;

	if ((ref->type & GIT_REF_SYMBOLIC) == 0)
		return GIT_EINVALIDREFSTATE;

	ref_sym = (reference_symbolic *)ref;

	free(ref_sym->target);
	ref_sym->target = git__strdup(target);
	if (ref_sym->target == NULL)
		return GIT_ENOMEM;

	return reference_write(ref);
}

const git_oid *git_reference_oid(git_reference *ref)
{
	assert(ref);

	if ((ref->type & GIT_REF_OID) == 0)
		return NULL;

	return &((reference_oid *)ref)->oid;
}

const char *git_reference_target(git_reference *ref)
{
	assert(ref);

	if ((ref->type & GIT_REF_SYMBOLIC) == 0)
		return NULL;

	return ((reference_symbolic *)ref)->target;
}

git_rtype git_reference_type(git_reference *ref)
{
	assert(ref);

	if (ref->type & GIT_REF_OID)
		return GIT_REF_OID;

	if (ref->type & GIT_REF_SYMBOLIC)
		return GIT_REF_SYMBOLIC;

	return GIT_REF_INVALID;
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
		reference_symbolic *ref_sym;

		if (ref->type & GIT_REF_OID) {
			*resolved_ref = ref;
			return GIT_SUCCESS;
		}

		ref_sym = (reference_symbolic *)ref;
		if ((error = git_repository_lookup_ref(&ref, repo, ref_sym->target)) < GIT_SUCCESS)
			return error;
	}

	return GIT_ETOONESTEDSYMREF;
}

static int reference_write(git_reference *ref)
{
	git_filebuf file;
	char ref_path[GIT_PATH_MAX];
	int error, contents_size;
	char *ref_contents = NULL;

	git__joinpath(ref_path, ref->owner->path_repository, ref->name);

	if ((error = git_filebuf_open(&file, ref_path, 0)) < GIT_SUCCESS)
		return error;

	if (ref->type & GIT_REF_OID) {
		reference_oid *ref_oid = (reference_oid *)ref;

		contents_size = GIT_OID_HEXSZ + 1;
		ref_contents = git__malloc(contents_size);
		if (ref_contents == NULL) {
			error = GIT_ENOMEM;
			goto unlock;
		}

		git_oid_fmt(ref_contents, &ref_oid->oid);

	} else if (ref->type & GIT_REF_SYMBOLIC) { /* GIT_REF_SYMBOLIC */
		reference_symbolic *ref_sym = (reference_symbolic *)ref;

		contents_size = strlen(GIT_SYMREF) + strlen(ref_sym->target) + 1;
		ref_contents = git__malloc(contents_size);
		if (ref_contents == NULL) {
			error = GIT_ENOMEM;
			goto unlock;
		}

		strcpy(ref_contents, GIT_SYMREF);
		strcat(ref_contents, ref_sym->target);
	} else {
		error = GIT_EINVALIDREFSTATE;
		goto unlock;
	}

	/* TODO: win32 carriage return when writing references in Windows? */
	ref_contents[contents_size - 1] = '\n';

	if ((error = git_filebuf_write(&file, ref_contents, contents_size)) < GIT_SUCCESS)
		goto unlock;

	error = git_filebuf_commit(&file);

	free(ref_contents);
	return error;

unlock:
	git_filebuf_cleanup(&file);
	free(ref_contents);
	return error;
}

int git_repository_lookup_ref(git_reference **ref_out, git_repository *repo, const char *name)
{
	int error;
	char normalized_name[GIT_PATH_MAX];

	assert(ref_out && repo && name);

	*ref_out = NULL;

	error = normalize_name(normalized_name, name, 0);
	if (error < GIT_SUCCESS)
		return error;

	/*
	 * First, check if the reference is on the local cache;
	 * references on the cache are assured to be up-to-date
	 */
	*ref_out = git_hashtable_lookup(repo->references.loose_refs, normalized_name);
	if (*ref_out != NULL)
		return GIT_SUCCESS;

	/*
	 * Then check if there is a loose file for that reference.
	 * If the file exists, we parse it and store it on the
	 * cache.
	 */
	error = lookup_loose_ref(ref_out, repo, normalized_name);

	if (error == GIT_SUCCESS)
		return GIT_SUCCESS;

	if (error != GIT_ENOTFOUND)
		return error;

	if (!repo->references.pack_loaded) {
		/* load all the packed references */
		error = load_packed_refs(&repo->references, repo);
		if (error < GIT_SUCCESS)
			return error;
	}

	*ref_out = git_hashtable_lookup(repo->references.packed_refs, normalized_name);
	if (*ref_out != NULL)
		return GIT_SUCCESS;

	/* The reference doesn't exist anywhere */
	return GIT_ENOTFOUND;
}

int git_repository__refcache_init(git_refcache *refs)
{
	assert(refs);

	refs->loose_refs = git_hashtable_alloc(
		default_table_size,
		reftable_hash,
		(git_hash_keyeq_ptr)strcmp);

	refs->packed_refs = git_hashtable_alloc(
		default_table_size, 
		reftable_hash,
		(git_hash_keyeq_ptr)strcmp);

	return (refs->loose_refs && refs->packed_refs) ? GIT_SUCCESS : GIT_ENOMEM;
}

void git_repository__refcache_free(git_refcache *refs)
{
	git_reference *reference;
	const void *_unused;

	assert(refs);

	GIT_HASHTABLE_FOREACH(refs->loose_refs, _unused, reference,
		reference_free(reference);
	);

	GIT_HASHTABLE_FOREACH(refs->packed_refs, _unused, reference,
		reference_free(reference);
	);

	git_hashtable_free(refs->loose_refs);
	git_hashtable_free(refs->packed_refs);
}

static int check_valid_ref_char(char ch)
{
	if (ch <= ' ')
		return GIT_ERROR;

	switch (ch) {
	case '~':
	case '^':
	case ':':
	case '\\':
	case '?':
	case '[':
	case '*':
		return GIT_ERROR;
		break;

	default:
		return GIT_SUCCESS;
	}
}


static int normalize_name(char *buffer_out, const char *name, int is_oid_ref)
{
	int error = GIT_SUCCESS;
	const char *name_end, *buffer_out_start;
	char *current;
	int contains_a_slash = 0;

	assert(name && buffer_out);

	buffer_out_start = buffer_out;
	current = (char *)name;
	name_end = name + strlen(name);

	/* A refname can not be empty */
	if (name_end == name)
		return GIT_EINVALIDREFNAME;

	/* A refname can not end with a dot or a slash */
	if (*(name_end - 1) == '.' || *(name_end - 1) == '/')
		return GIT_EINVALIDREFNAME;

	while (current < name_end) {
		if (check_valid_ref_char(*current))
				return GIT_EINVALIDREFNAME;

		if (buffer_out > buffer_out_start) {
			char prev = *(buffer_out - 1);

			/* A refname can not start with a dot nor contain a double dot */
			if (*current == '.' && ((prev == '.') || (prev == '/')))
				return GIT_EINVALIDREFNAME;

			/* '@{' is forbidden within a refname */
			if (*current == '{' && prev == '@')
				return GIT_EINVALIDREFNAME;

			/* Prevent multiple slashes from being added to the output */
			if (*current == '/' && prev == '/') {
				current++;
				continue;
			}
		}

		if (*current == '/')
			contains_a_slash = 1;

		*buffer_out++ = *current++;
	}

	/* Object id refname have to contain at least one slash */
	if (is_oid_ref && !contains_a_slash)
				return GIT_EINVALIDREFNAME;

	/* A refname can not end with ".lock" */
	if (!git__suffixcmp(name, GIT_FILELOCK_EXTENSION))
				return GIT_EINVALIDREFNAME;

	*buffer_out = '\0';

	/* For object id references, name has to start with refs/(heads|tags|remotes) */
	if (is_oid_ref && !(!git__prefixcmp(buffer_out_start, GIT_REFS_HEADS_DIR) ||
			!git__prefixcmp(buffer_out_start, GIT_REFS_TAGS_DIR) || !git__prefixcmp(buffer_out_start, GIT_REFS_REMOTES_DIR)))
		return GIT_EINVALIDREFNAME;

	return error;
}

int git_reference__normalize_name(char *buffer_out, const char *name)
{
	return normalize_name(buffer_out, name, 0);
}

int git_reference__normalize_name_oid(char *buffer_out, const char *name)
{
	return normalize_name(buffer_out, name, 1);
}


