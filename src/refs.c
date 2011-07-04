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

#include <git2/tag.h>
#include <git2/object.h>

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

static void reference_free(git_reference *reference);
static int reference_create(git_reference **ref_out, git_repository *repo, const char *name, git_rtype type);
static int reference_read(git_fbuffer *file_content, time_t *mtime, const char *repo_path, const char *ref_name);

/* loose refs */
static int loose_parse_symbolic(git_reference *ref, git_fbuffer *file_content);
static int loose_parse_oid(git_reference *ref, git_fbuffer *file_content);
static int loose_lookup(git_reference **ref_out, git_repository *repo, const char *name, int skip_symbolic);
static int loose_write(git_reference *ref);
static int loose_update(git_reference *ref);

/* packed refs */
static int packed_parse_peel(reference_oid *tag_ref, const char **buffer_out, const char *buffer_end);
static int packed_parse_oid(reference_oid **ref_out, git_repository *repo, const char **buffer_out, const char *buffer_end);
static int packed_load(git_repository *repo);
static int packed_loadloose(git_repository *repository);
static int packed_write_ref(reference_oid *ref, git_filebuf *file);
static int packed_find_peel(reference_oid *ref);
static int packed_remove_loose(git_repository *repo, git_vector *packing_list);
static int packed_sort(const void *a, const void *b);
static int packed_write(git_repository *repo);

/* internal helpers */
static int reference_available(git_repository *repo, const char *ref, const char *old_ref);

/* name normalization */
static int check_valid_ref_char(char ch);
static int normalize_name(char *buffer_out, size_t out_size, const char *name, int is_oid_ref);

/*****************************************
 * Internal methods - Constructor/destructor
 *****************************************/
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

static int reference_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	git_rtype type)
{
	char normalized[GIT_REFNAME_MAX];
	int error = GIT_SUCCESS, size;
	git_reference *reference = NULL;

	assert(ref_out && repo && name);

	if (type == GIT_REF_SYMBOLIC)
		size = sizeof(reference_symbolic);
	else if (type == GIT_REF_OID)
		size = sizeof(reference_oid);
	else
		return git__throw(GIT_EINVALIDARGS,
			"Invalid reference type. Use either GIT_REF_OID or GIT_REF_SYMBOLIC as type specifier");

	reference = git__malloc(size);
	if (reference == NULL)
		return GIT_ENOMEM;

	memset(reference, 0x0, size);
	reference->owner = repo;
	reference->type = type;

	error = normalize_name(normalized, sizeof(normalized), name, (type & GIT_REF_OID));
	if (error < GIT_SUCCESS)
		goto cleanup;

	reference->name = git__strdup(normalized);
	if (reference->name == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	*ref_out = reference;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create reference");

cleanup:
	reference_free(reference);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create reference");
}

static int reference_read(git_fbuffer *file_content, time_t *mtime, const char *repo_path, const char *ref_name)
{
	struct stat st;
	char path[GIT_PATH_MAX];

	/* Determine the full path of the file */
	git_path_join(path, repo_path, ref_name);

	if (p_stat(path, &st) < 0 || S_ISDIR(st.st_mode))
		return git__throw(GIT_ENOTFOUND,
			"Cannot read reference file '%s'", ref_name);

	if (mtime)
		*mtime = st.st_mtime;

	if (file_content)
		return git_futils_readbuffer(file_content, path);

	return GIT_SUCCESS;
}




/*****************************************
 * Internal methods - Loose references
 *****************************************/
static int loose_update(git_reference *ref)
{
	int error;
	time_t ref_time;
	git_fbuffer ref_file = GIT_FBUFFER_INIT;

	if (ref->type & GIT_REF_PACKED)
		return packed_load(ref->owner);

	error = reference_read(NULL, &ref_time, ref->owner->path_repository, ref->name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (ref_time == ref->mtime)
		return GIT_SUCCESS;

	error = reference_read(&ref_file, &ref->mtime, ref->owner->path_repository, ref->name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (ref->type == GIT_REF_SYMBOLIC)
		error = loose_parse_symbolic(ref, &ref_file);
	else if (ref->type == GIT_REF_OID)
		error = loose_parse_oid(ref, &ref_file);
	else
		error = git__throw(GIT_EOBJCORRUPTED,
			"Invalid reference type (%d) for loose reference", ref->type);

	git_futils_freebuffer(&ref_file);

cleanup:
	if (error != GIT_SUCCESS) {
		reference_free(ref);
		git_hashtable_remove(ref->owner->references.loose_cache, ref->name);
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to update loose reference");
}

static int loose_parse_symbolic(git_reference *ref, git_fbuffer *file_content)
{
	const unsigned int header_len = strlen(GIT_SYMREF);
	const char *refname_start;
	char *eol;
	reference_symbolic *ref_sym;

	refname_start = (const char *)file_content->data;
	ref_sym = (reference_symbolic *)ref;

	if (file_content->len < (header_len + 1))
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Object too short");

	/*
	 * Assume we have already checked for the header
	 * before calling this function
	 */

	refname_start += header_len;

	free(ref_sym->target);
	ref_sym->target = git__strdup(refname_start);
	if (ref_sym->target == NULL)
		return GIT_ENOMEM;

	/* remove newline at the end of file */
	eol = strchr(ref_sym->target, '\n');
	if (eol == NULL)
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Missing EOL");

	*eol = '\0';
	if (eol[-1] == '\r')
		eol[-1] = '\0';

	return GIT_SUCCESS;
}

static int loose_parse_oid(git_reference *ref, git_fbuffer *file_content)
{
	int error;
	reference_oid *ref_oid;
	char *buffer;

	buffer = (char *)file_content->data;
	ref_oid = (reference_oid *)ref;

	/* File format: 40 chars (OID) + newline */
	if (file_content->len < GIT_OID_HEXSZ + 1)
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Reference too short");

	if ((error = git_oid_fromstr(&ref_oid->oid, buffer)) < GIT_SUCCESS)
		return git__rethrow(GIT_EOBJCORRUPTED, "Failed to parse loose reference.");

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Missing EOL");

	return GIT_SUCCESS;
}


static git_rtype loose_guess_rtype(const char *full_path)
{
	git_fbuffer ref_file = GIT_FBUFFER_INIT;
	git_rtype type;

	type = GIT_REF_INVALID;

	if (git_futils_readbuffer(&ref_file, full_path) == GIT_SUCCESS) {
		if (git__prefixcmp((const char *)(ref_file.data), GIT_SYMREF) == 0)
			type = GIT_REF_SYMBOLIC;
		else
			type = GIT_REF_OID;
	}

	git_futils_freebuffer(&ref_file);
	return type;
}

static int loose_lookup(
		git_reference **ref_out,
		git_repository *repo,
		const char *name,
		int skip_symbolic)
{
	int error = GIT_SUCCESS;
	git_fbuffer ref_file = GIT_FBUFFER_INIT;
	git_reference *ref = NULL;
	time_t ref_time;

	*ref_out = NULL;

	error = reference_read(&ref_file, &ref_time, repo->path_repository, name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (git__prefixcmp((const char *)(ref_file.data), GIT_SYMREF) == 0) {
		if (skip_symbolic)
			return GIT_SUCCESS;

		error = reference_create(&ref, repo, name, GIT_REF_SYMBOLIC);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = loose_parse_symbolic(ref, &ref_file);
	} else {
		error = reference_create(&ref, repo, name, GIT_REF_OID);
		if (error < GIT_SUCCESS)
			goto cleanup;

		error = loose_parse_oid(ref, &ref_file);
	}

	if (error < GIT_SUCCESS)
		goto cleanup;

	ref->mtime = ref_time;
	*ref_out = ref;
	git_futils_freebuffer(&ref_file);
	return GIT_SUCCESS;

cleanup:
	git_futils_freebuffer(&ref_file);
	reference_free(ref);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to lookup loose reference");
}

static int loose_write(git_reference *ref)
{
	git_filebuf file;
	char ref_path[GIT_PATH_MAX];
	int error;
	struct stat st;

	git_path_join(ref_path, ref->owner->path_repository, ref->name);

	if ((error = git_filebuf_open(&file, ref_path, GIT_FILEBUF_FORCE)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write loose reference");

	if (ref->type & GIT_REF_OID) {
		reference_oid *ref_oid = (reference_oid *)ref;
		char oid[GIT_OID_HEXSZ + 1];

		memset(oid, 0x0, sizeof(oid));

		git_oid_fmt(oid, &ref_oid->oid);
		error = git_filebuf_printf(&file, "%s\n", oid);
		if (error < GIT_SUCCESS)
			goto unlock;

	} else if (ref->type & GIT_REF_SYMBOLIC) { /* GIT_REF_SYMBOLIC */
		reference_symbolic *ref_sym = (reference_symbolic *)ref;

		error = git_filebuf_printf(&file, GIT_SYMREF "%s\n", ref_sym->target);
	} else {
		error = git__throw(GIT_EOBJCORRUPTED, "Failed to write reference. Invalid reference type");
		goto unlock;
	}

	error = git_filebuf_commit(&file);

	if (p_stat(ref_path, &st) == GIT_SUCCESS)
		ref->mtime = st.st_mtime;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write loose reference");

unlock:
	git_filebuf_cleanup(&file);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write loose reference");
}






/*****************************************
 * Internal methods - Packed references
 *****************************************/

static int packed_parse_peel(
		reference_oid *tag_ref,
		const char **buffer_out,
		const char *buffer_end)
{
	const char *buffer = *buffer_out + 1;

	assert(buffer[-1] == '^');

	/* Ensure it's not the first entry of the file */
	if (tag_ref == NULL)
		return git__throw(GIT_EPACKEDREFSCORRUPTED, "Failed to parse packed reference. Reference is the first entry of the file");

	/* Ensure reference is a tag */
	if (git__prefixcmp(tag_ref->ref.name, GIT_REFS_TAGS_DIR) != 0)
		return git__throw(GIT_EPACKEDREFSCORRUPTED, "Failed to parse packed reference. Reference is not a tag");

	if (buffer + GIT_OID_HEXSZ >= buffer_end)
		return git__throw(GIT_EPACKEDREFSCORRUPTED, "Failed to parse packed reference. Buffer too small");

	/* Is this a valid object id? */
	if (git_oid_fromstr(&tag_ref->peel_target, buffer) < GIT_SUCCESS)
		return git__throw(GIT_EPACKEDREFSCORRUPTED, "Failed to parse packed reference. Not a valid object ID");

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return git__throw(GIT_EPACKEDREFSCORRUPTED, "Failed to parse packed reference. Buffer not terminated correctly");

	*buffer_out = buffer + 1;
	tag_ref->ref.type |= GIT_REF_HAS_PEEL;

	return GIT_SUCCESS;
}

static int packed_parse_oid(
		reference_oid **ref_out,
		git_repository *repo,
		const char **buffer_out,
		const char *buffer_end)
{
	reference_oid *ref = NULL;

	const char *buffer = *buffer_out;
	const char *refname_begin, *refname_end;

	int error = GIT_SUCCESS;
	int refname_len;
	char refname[GIT_REFNAME_MAX];
	git_oid id;

	refname_begin = (buffer + GIT_OID_HEXSZ + 1);
	if (refname_begin >= buffer_end ||
		refname_begin[-1] != ' ') {
		error = GIT_EPACKEDREFSCORRUPTED;
		goto cleanup;
	}

	/* Is this a valid object id? */
	if ((error = git_oid_fromstr(&id, buffer)) < GIT_SUCCESS)
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
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse OID of packed reference");
}

static int packed_load(git_repository *repo)
{
	int error = GIT_SUCCESS;
	git_fbuffer packfile = GIT_FBUFFER_INIT;
	const char *buffer_start, *buffer_end;
	git_refcache *ref_cache = &repo->references;

	/* already loaded */
	if (repo->references.packfile != NULL) {
		time_t packed_time;

		/* check if we can read the time of the index;
		 * if we can read it and it matches the time of the
		 * index we had previously loaded, we don't need to do
		 * anything else.
		 *
		 * if we cannot load the time (e.g. the packfile
		 * has disappeared) or the time is different, we
		 * have to reload the packfile */

		if (!reference_read(NULL, &packed_time, repo->path_repository, GIT_PACKEDREFS_FILE) &&
			packed_time == ref_cache->packfile_time)
			return GIT_SUCCESS;

		git_hashtable_clear(repo->references.packfile);
	} else {
		ref_cache->packfile = git_hashtable_alloc(
			default_table_size,
			reftable_hash,
			(git_hash_keyeq_ptr)strcmp);

		if (ref_cache->packfile == NULL)
			return GIT_ENOMEM;
	}

	/* read the packfile from disk;
	 * store its modification time to check for future reloads */
	error = reference_read(
			&packfile,
			&ref_cache->packfile_time,
			repo->path_repository,
			GIT_PACKEDREFS_FILE);

	/* there is no packfile on disk; that's ok */
	if (error == GIT_ENOTFOUND)
		return GIT_SUCCESS;

	if (error < GIT_SUCCESS)
		goto cleanup;

	buffer_start = (const char *)packfile.data;
	buffer_end = (const char *)(buffer_start) + packfile.len;

	while (buffer_start < buffer_end && buffer_start[0] == '#') {
		buffer_start = strchr(buffer_start, '\n');
		if (buffer_start == NULL) {
			error = GIT_EPACKEDREFSCORRUPTED;
			goto cleanup;
		}
		buffer_start++;
	}

	while (buffer_start < buffer_end) {
		reference_oid *ref = NULL;

		error = packed_parse_oid(&ref, repo, &buffer_start, buffer_end);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if (buffer_start[0] == '^') {
			error = packed_parse_peel(ref, &buffer_start, buffer_end);
			if (error < GIT_SUCCESS)
				goto cleanup;
		}

		error = git_hashtable_insert(ref_cache->packfile, ref->ref.name, ref);
		if (error < GIT_SUCCESS) {
			reference_free((git_reference *)ref);
			goto cleanup;
		}
	}

	git_futils_freebuffer(&packfile);
	return GIT_SUCCESS;

cleanup:
	git_hashtable_free(ref_cache->packfile);
	ref_cache->packfile = NULL;
	git_futils_freebuffer(&packfile);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to load packed references");
}




struct dirent_list_data {
	git_repository *repo;
	size_t repo_path_len;
	unsigned int list_flags;

	int (*callback)(const char *, void *);
	void *callback_payload;
};

static int _dirent_loose_listall(void *_data, char *full_path)
{
	struct dirent_list_data *data = (struct dirent_list_data *)_data;
	char *file_path = full_path + data->repo_path_len;

	if (git_futils_isdir(full_path) == GIT_SUCCESS)
		return git_futils_direach(full_path, GIT_PATH_MAX, _dirent_loose_listall, _data);

	/* do not add twice a reference that exists already in the packfile */
	if ((data->list_flags & GIT_REF_PACKED) != 0 &&
		git_hashtable_lookup(data->repo->references.packfile, file_path) != NULL)
		return GIT_SUCCESS;

	if (data->list_flags != GIT_REF_LISTALL) {
		if ((data->list_flags & loose_guess_rtype(full_path)) == 0)
			return GIT_SUCCESS; /* we are filtering out this reference */
	}

	return data->callback(file_path, data->callback_payload);
}

static int _dirent_loose_load(void *data, char *full_path)
{
	git_repository *repository = (git_repository *)data;
	git_reference *reference, *old_ref;
	char *file_path;
	int error;

	if (git_futils_isdir(full_path) == GIT_SUCCESS)
		return git_futils_direach(full_path, GIT_PATH_MAX, _dirent_loose_load, repository);

	file_path = full_path + strlen(repository->path_repository);
	error = loose_lookup(&reference, repository, file_path, 1);
	if (error == GIT_SUCCESS && reference != NULL) {
		reference->type |= GIT_REF_PACKED;

		if (git_hashtable_insert2(repository->references.packfile, reference->name, reference, (void **)&old_ref) < GIT_SUCCESS) {
			reference_free(reference);
			return GIT_ENOMEM;
		}

		if (old_ref != NULL)
			reference_free(old_ref);
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to load loose dirent");
}

/*
 * Load all the loose references from the repository
 * into the in-memory Packfile, and build a vector with
 * all the references so it can be written back to
 * disk.
 */
static int packed_loadloose(git_repository *repository)
{
	char refs_path[GIT_PATH_MAX];

	/* the packfile must have been previously loaded! */
	assert(repository->references.packfile);

	git_path_join(refs_path, repository->path_repository, GIT_REFS_DIR);

	/* Remove any loose references from the cache */
	{
		const void *GIT_UNUSED(_unused);
		git_reference *reference;

		GIT_HASHTABLE_FOREACH(repository->references.loose_cache, _unused, reference,
			reference_free(reference);
		);
	}

	git_hashtable_clear(repository->references.loose_cache);

	/*
	 * Load all the loose files from disk into the Packfile table.
	 * This will overwrite any old packed entries with their
	 * updated loose versions
	 */
	return git_futils_direach(refs_path, GIT_PATH_MAX, _dirent_loose_load, repository);
}

/*
 * Write a single reference into a packfile
 */
static int packed_write_ref(reference_oid *ref, git_filebuf *file)
{
	int error;
	char oid[GIT_OID_HEXSZ + 1];

	git_oid_fmt(oid, &ref->oid);
	oid[GIT_OID_HEXSZ] = 0;

	/*
	 * For references that peel to an object in the repo, we must
	 * write the resulting peel on a separate line, e.g.
	 *
	 *	6fa8a902cc1d18527e1355773c86721945475d37 refs/tags/libgit2-0.4
	 *	^2ec0cb7959b0bf965d54f95453f5b4b34e8d3100
	 *
	 * This obviously only applies to tags.
	 * The required peels have already been loaded into `ref->peel_target`.
	 */
	if (ref->ref.type & GIT_REF_HAS_PEEL) {
		char peel[GIT_OID_HEXSZ + 1];
		git_oid_fmt(peel, &ref->peel_target);
		peel[GIT_OID_HEXSZ] = 0;

		error = git_filebuf_printf(file, "%s %s\n^%s\n", oid, ref->ref.name, peel);
	} else {
		error = git_filebuf_printf(file, "%s %s\n", oid, ref->ref.name);
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write packed reference");
}

/*
 * Find out what object this reference resolves to.
 *
 * For references that point to a 'big' tag (e.g. an
 * actual tag object on the repository), we need to
 * cache on the packfile the OID of the object to
 * which that 'big tag' is pointing to.
 */
static int packed_find_peel(reference_oid *ref)
{
	git_object *object;
	int error;

	if (ref->ref.type & GIT_REF_HAS_PEEL)
		return GIT_SUCCESS;

	/*
	 * Only applies to tags, i.e. references
	 * in the /refs/tags folder
	 */
	if (git__prefixcmp(ref->ref.name, GIT_REFS_TAGS_DIR) != 0)
		return GIT_SUCCESS;

	/*
	 * Find the tagged object in the repository
	 */
	error = git_object_lookup(&object, ref->ref.owner, &ref->oid, GIT_OBJ_ANY);
	if (error < GIT_SUCCESS)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to find packed reference");

	/*
	 * If the tagged object is a Tag object, we need to resolve it;
	 * if the ref is actually a 'weak' ref, we don't need to resolve
	 * anything.
	 */
	if (git_object_type(object) == GIT_OBJ_TAG) {
		git_tag *tag = (git_tag *)object;

		/*
		 * Find the object pointed at by this tag
		 */
		git_oid_cpy(&ref->peel_target, git_tag_target_oid(tag));
		ref->ref.type |= GIT_REF_HAS_PEEL;

		/*
		 * The reference has now cached the resolved OID, and is
		 * marked at such. When written to the packfile, it'll be
		 * accompanied by this resolved oid
		 */
	}

	git_object_close(object);

	return GIT_SUCCESS;
}

/*
 * Remove all loose references
 *
 * Once we have successfully written a packfile,
 * all the loose references that were packed must be
 * removed from disk.
 *
 * This is a dangerous method; make sure the packfile
 * is well-written, because we are destructing references
 * here otherwise.
 */
static int packed_remove_loose(git_repository *repo, git_vector *packing_list)
{
	unsigned int i;
	char full_path[GIT_PATH_MAX];
	int error = GIT_SUCCESS;
	git_reference *reference;

	for (i = 0; i < packing_list->length; ++i) {
		git_reference *ref = git_vector_get(packing_list, i);

		/* Ensure the packed reference doesn't exist
		 * in a (more up-to-date?) state as a loose reference
		 */
		reference = git_hashtable_lookup(ref->owner->references.loose_cache, ref->name);
		if (reference != NULL)
			continue;

		git_path_join(full_path, repo->path_repository, ref->name);

		if (git_futils_exists(full_path) == GIT_SUCCESS &&
			p_unlink(full_path) < GIT_SUCCESS)
			error = GIT_EOSERR;

		/*
		 * if we fail to remove a single file, this is *not* good,
		 * but we should keep going and remove as many as possible.
		 * After we've removed as many files as possible, we return
		 * the error code anyway.
		 *
		 * TODO: mark this with a very special error code?
		 * GIT_EFAILTORMLOOSE
		 */
	}

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to remove loose packed reference");
}

static int packed_sort(const void *a, const void *b)
{
	const git_reference *ref_a = *(const git_reference **)a;
	const git_reference *ref_b = *(const git_reference **)b;

	return strcmp(ref_a->name, ref_b->name);
}

/*
 * Write all the contents in the in-memory packfile to disk.
 */
static int packed_write(git_repository *repo)
{
	git_filebuf pack_file;
	int error;
	unsigned int i;
	char pack_file_path[GIT_PATH_MAX];

	git_vector packing_list;
	size_t total_refs;

	assert(repo && repo->references.packfile);

	total_refs = repo->references.packfile->key_count;
	if ((error = git_vector_init(&packing_list, total_refs, packed_sort)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write packed reference");

	/* Load all the packfile into a vector */
	{
		git_reference *reference;
		const void *GIT_UNUSED(_unused);

		GIT_HASHTABLE_FOREACH(repo->references.packfile, _unused, reference,
			git_vector_insert(&packing_list, reference);  /* cannot fail: vector already has the right size */
		);
	}

	/* sort the vector so the entries appear sorted on the packfile */
	git_vector_sort(&packing_list);

	/* Now we can open the file! */
	git_path_join(pack_file_path, repo->path_repository, GIT_PACKEDREFS_FILE);
	if ((error = git_filebuf_open(&pack_file, pack_file_path, 0)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write packed reference");

	/* Packfiles have a header... apparently
	 * This is in fact not required, but we might as well print it
	 * just for kicks */
	if ((error = git_filebuf_printf(&pack_file, "%s\n", GIT_PACKEDREFS_HEADER)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write packed reference");

	for (i = 0; i < packing_list.length; ++i) {
		reference_oid *ref = (reference_oid *)git_vector_get(&packing_list, i);

		/* only direct references go to the packfile; otherwise
		 * this is a disaster */
		assert(ref->ref.type & GIT_REF_OID);

		if ((error = packed_find_peel(ref)) < GIT_SUCCESS) {
			error = git__throw(GIT_EOBJCORRUPTED, "A reference cannot be peeled");
			goto cleanup;
		}


		if ((error = packed_write_ref(ref, &pack_file)) < GIT_SUCCESS)
			goto cleanup;
	}

cleanup:
	/* if we've written all the references properly, we can commit
	 * the packfile to make the changes effective */
	if (error == GIT_SUCCESS) {
		error = git_filebuf_commit(&pack_file);

		/* when and only when the packfile has been properly written,
		 * we can go ahead and remove the loose refs */
		if (error == GIT_SUCCESS) {
			struct stat st;

			error = packed_remove_loose(repo, &packing_list);

			if (p_stat(pack_file_path, &st) == GIT_SUCCESS)
				repo->references.packfile_time = st.st_mtime;
		}
	}
	else git_filebuf_cleanup(&pack_file);

	git_vector_free(&packing_list);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write packed reference");
}

static int _reference_available_cb(const char *ref, void *data)
{
	const char *new, *old;
	git_vector *refs;

	assert(ref && data);

	refs = (git_vector *)data;

	new = (const char *)git_vector_get(refs, 0);
	old = (const char *)git_vector_get(refs, 1);

	if (!old || strcmp(old, ref)) {
		int reflen = strlen(ref);
		int newlen = strlen(new);
		int cmplen = reflen < newlen ? reflen : newlen;
		const char *lead = reflen < newlen ? new : ref;

		if (!strncmp(new, ref, cmplen) &&
		    lead[cmplen] == '/')
			return GIT_EEXISTS;
	}

	return GIT_SUCCESS;
}

static int reference_available(git_repository *repo, const char *ref, const char* old_ref)
{
	int error;
	git_vector refs;

	if (git_vector_init(&refs, 2, NULL) < GIT_SUCCESS)
		return GIT_ENOMEM;

	git_vector_insert(&refs, (void *)ref);
	git_vector_insert(&refs, (void *)old_ref);

	error = git_reference_foreach(repo, GIT_REF_LISTALL, _reference_available_cb, (void *)&refs);

	git_vector_free(&refs);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__throw(GIT_EEXISTS, "Reference name `%s` conflicts with existing reference", ref);
}

/*****************************************
 * External Library API
 *****************************************/

/**
 * Constructors
 */
int git_reference_lookup(git_reference **ref_out, git_repository *repo, const char *name)
{
	int error;
	char normalized_name[GIT_REFNAME_MAX];

	assert(ref_out && repo && name);

	*ref_out = NULL;

	error = normalize_name(normalized_name, sizeof(normalized_name), name, 0);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference");

	/* First, check has been previously loaded and cached */
	*ref_out = git_hashtable_lookup(repo->references.loose_cache, normalized_name);
	if (*ref_out != NULL)
		return loose_update(*ref_out);

	/* Then check if there is a loose file for that reference.*/
	error = loose_lookup(ref_out, repo, normalized_name, 0);

	/* If the file exists, we store it on the cache */
	if (error == GIT_SUCCESS)
		return git_hashtable_insert(repo->references.loose_cache, (*ref_out)->name, (*ref_out));

	/* The loose lookup has failed, but not because the reference wasn't found;
	 * probably the loose reference is corrupted. this is bad. */
	if (error != GIT_ENOTFOUND)
		return git__rethrow(error, "Failed to lookup reference");

	/*
	 * If we cannot find a loose reference, we look into the packfile
	 * Load the packfile first if it hasn't been loaded
	 */
	/* load all the packed references */
	error = packed_load(repo);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference");

	/* Look up on the packfile */
	*ref_out = git_hashtable_lookup(repo->references.packfile, normalized_name);
	if (*ref_out != NULL)
		return GIT_SUCCESS;

	/* The reference doesn't exist anywhere */
	return git__throw(GIT_ENOTFOUND, "Failed to lookup reference. Reference doesn't exist");
}

/**
 * Getters
 */
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

const git_oid *git_reference_oid(git_reference *ref)
{
	assert(ref);

	if ((ref->type & GIT_REF_OID) == 0)
		return NULL;

	if (loose_update(ref) < GIT_SUCCESS)
		return NULL;

	return &((reference_oid *)ref)->oid;
}

const char *git_reference_target(git_reference *ref)
{
	assert(ref);

	if ((ref->type & GIT_REF_SYMBOLIC) == 0)
		return NULL;

	if (loose_update(ref) < GIT_SUCCESS)
		return NULL;

	return ((reference_symbolic *)ref)->target;
}

int git_reference_create_symbolic(git_reference **ref_out, git_repository *repo, const char *name, const char *target, int force)
{
	char normalized[GIT_REFNAME_MAX];
	int error = GIT_SUCCESS, updated = 0;
	git_reference *ref = NULL, *old_ref = NULL;

	if (git_reference_lookup(&ref, repo, name) == GIT_SUCCESS && !force)
		return git__throw(GIT_EEXISTS, "Failed to create symbolic reference. Reference already exists");

	/*
	 * If they old ref was of the same type, then we can just update
	 * it (once we've checked that the target is valid). Otherwise we
	 * need a new reference because we can't make a symbolic ref out
	 * of an oid one.
	 * If if didn't exist, then we need to create a new one anyway.
     */
	if (ref && ref->type & GIT_REF_SYMBOLIC){
		updated = 1;
	} else {
		ref = NULL;
		error = reference_create(&ref, repo, name, GIT_REF_SYMBOLIC);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	/* The target can aither be the name of an object id reference or the name of another symbolic reference */
	error = normalize_name(normalized, sizeof(normalized), target, 0);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* set the target; this will write the reference on disk */
	error = git_reference_set_target(ref, normalized);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/*
	 * If we didn't update the ref, then we need to insert or replace
	 * it in the loose cache. If we replaced a ref, free it.
	 */
	if (!updated){
		error = git_hashtable_insert2(repo->references.loose_cache, ref->name, ref, (void **) &old_ref);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if(old_ref)
			reference_free(old_ref);
	}

	*ref_out = ref;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create symbolic reference");

cleanup:
	reference_free(ref);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create symbolic reference");
}

int git_reference_create_oid(git_reference **ref_out, git_repository *repo, const char *name, const git_oid *id, int force)
{
	int error = GIT_SUCCESS, updated = 0;
	git_reference *ref = NULL, *old_ref = NULL;

	if(git_reference_lookup(&ref, repo, name) == GIT_SUCCESS && !force)
		return git__throw(GIT_EEXISTS, "Failed to create reference OID. Reference already exists");

	if ((error = reference_available(repo, name, NULL)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create reference");

	/*
	 * If they old ref was of the same type, then we can just update
	 * it (once we've checked that the target is valid). Otherwise we
	 * need a new reference because we can't make a symbolic ref out
	 * of an oid one.
	 * If if didn't exist, then we need to create a new one anyway.
     */
	if (ref && ref-> type & GIT_REF_OID){
		updated = 1;
	} else {
		ref = NULL;
		error = reference_create(&ref, repo, name, GIT_REF_OID);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	/* set the oid; this will write the reference on disk */
	error = git_reference_set_oid(ref, id);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if(!updated){
		error = git_hashtable_insert2(repo->references.loose_cache, ref->name, ref, (void **) &old_ref);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if(old_ref)
			reference_free(old_ref);
	}

	*ref_out = ref;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create reference OID");

cleanup:
	reference_free(ref);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to create reference OID");
}

/**
 * Setters
 */

/*
 * Change the OID target of a reference.
 *
 * For loose references, just change the oid in memory
 * and overwrite the file in disk.
 *
 * For packed files, this is not pretty:
 * For performance reasons, we write the new reference
 * loose on disk (it replaces the old on the packfile),
 * but we cannot invalidate the pointer to the reference,
 * and most importantly, the `packfile` object must stay
 * consistent with the representation of the packfile
 * on disk. This is what we need to:
 *
 * 1. Copy the reference
 * 2. Change the oid on the original
 * 3. Write the original to disk
 * 4. Write the original to the loose cache
 * 5. Replace the original with the copy (old reference) in the packfile cache
 */
int git_reference_set_oid(git_reference *ref, const git_oid *id)
{
	reference_oid *ref_oid;
	reference_oid *ref_old = NULL;
	int error = GIT_SUCCESS;

	if ((ref->type & GIT_REF_OID) == 0)
		return git__throw(GIT_EINVALIDREFSTATE, "Failed to set OID target of reference. Not an OID reference");

	ref_oid = (reference_oid *)ref;

	assert(ref->owner);

	/* Don't let the user create references to OIDs that
	 * don't exist in the ODB */
	if (!git_odb_exists(git_repository_database(ref->owner), id))
		return git__throw(GIT_ENOTFOUND, "Failed to set OID target of reference. OID doesn't exist in ODB");

	/* duplicate the reference;
	 * this copy will stay on the packfile cache */
	if (ref->type & GIT_REF_PACKED) {
		ref_old = git__malloc(sizeof(reference_oid));
		if (ref_old == NULL)
			return GIT_ENOMEM;

		ref_old->ref.name = git__strdup(ref->name);
		if (ref_old->ref.name == NULL) {
			free(ref_old);
			return GIT_ENOMEM;
		}
	}

	git_oid_cpy(&ref_oid->oid, id);
	ref->type &= ~GIT_REF_HAS_PEEL;

	error = loose_write(ref);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (ref->type & GIT_REF_PACKED) {
		/* insert the original on the loose cache */
		error = git_hashtable_insert(ref->owner->references.loose_cache, ref->name, ref);
		if (error < GIT_SUCCESS)
			goto cleanup;

		ref->type &= ~GIT_REF_PACKED;

		/* replace the original in the packfile with the copy */
		error = git_hashtable_insert(ref->owner->references.packfile, ref_old->ref.name, ref_old);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	return GIT_SUCCESS;

cleanup:
	reference_free((git_reference *)ref_old);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to set OID target of reference");
}

/*
 * Change the target of a symbolic reference.
 *
 * This is easy because symrefs cannot be inside
 * a pack. We just change the target in memory
 * and overwrite the file on disk.
 */
int git_reference_set_target(git_reference *ref, const char *target)
{
	reference_symbolic *ref_sym;

	if ((ref->type & GIT_REF_SYMBOLIC) == 0)
		return git__throw(GIT_EINVALIDREFSTATE, "Failed to set reference target. Not a symbolic reference");

	ref_sym = (reference_symbolic *)ref;

	free(ref_sym->target);
	ref_sym->target = git__strdup(target);
	if (ref_sym->target == NULL)
		return GIT_ENOMEM;

	return loose_write(ref);
}

/**
 * Other
 */

/*
 * Rename a reference
 *
 * If the reference is packed, we need to rewrite the
 * packfile to remove the reference from it and create
 * the reference back as a loose one.
 *
 * If the reference is loose, we just rename it on
 * the filesystem.
 *
 * We also need to re-insert the reference on its corresponding
 * in-memory cache, since the caches are indexed by refname.
 */
int git_reference_rename(git_reference *ref, const char *new_name, int force)
{
	int error;
	char *old_name;
	char old_path[GIT_PATH_MAX], new_path[GIT_PATH_MAX], normalized_name[GIT_REFNAME_MAX];
	git_reference *looked_up_ref, *old_ref = NULL;

	assert(ref);

	/* Ensure the name is valid */
	error = normalize_name(normalized_name, sizeof(normalized_name), new_name, ref->type & GIT_REF_OID);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to rename reference");

	new_name = normalized_name;

	/* Ensure we're not going to overwrite an existing reference
	   unless the user has allowed us */
	error = git_reference_lookup(&looked_up_ref, ref->owner, new_name);
	if (error == GIT_SUCCESS && !force)
		return git__throw(GIT_EEXISTS, "Failed to rename reference. Reference already exists");

	if (error < GIT_SUCCESS &&
	    error != GIT_ENOTFOUND)
		return git__rethrow(error, "Failed to rename reference");

	if ((error = reference_available(ref->owner, new_name, ref->name)) < GIT_SUCCESS)
		return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to rename reference. Reference already exists");

	old_name = ref->name;
	ref->name = git__strdup(new_name);

	if (ref->name == NULL) {
		ref->name = old_name;
		return GIT_ENOMEM;
	}

	if (ref->type & GIT_REF_PACKED) {
		/* write the packfile to disk; note
		 * that the state of the in-memory cache is not
		 * consistent, because the reference is indexed
		 * by its old name but it already has the new one.
		 * This doesn't affect writing, though, and allows
		 * us to rollback if writing fails
		 */

		/* Create the loose ref under its new name */
		error = loose_write(ref);
		if (error < GIT_SUCCESS) {
			ref->type |= GIT_REF_PACKED;
			goto cleanup;
		}

		ref->type &= ~GIT_REF_PACKED;

		/* Remove from the packfile cache in order to avoid packing it back
		 * Note : we do not rely on git_reference_delete() because this would
		 * invalidate the reference.
		 */
		git_hashtable_remove(ref->owner->references.packfile, old_name);

		/* Recreate the packed-refs file without the reference */
		error = packed_write(ref->owner);
		if (error < GIT_SUCCESS)
			goto rename_loose_to_old_name;

	} else {
		git_path_join(old_path, ref->owner->path_repository, old_name);
		git_path_join(new_path, ref->owner->path_repository, ref->name);

		error = git_futils_mv_withpath(old_path, new_path);
		if (error < GIT_SUCCESS)
			goto cleanup;

		/* Once succesfully renamed, remove from the cache the reference known by its old name*/
		git_hashtable_remove(ref->owner->references.loose_cache, old_name);
	}

	/* Store the renamed reference into the loose ref cache */
	error = git_hashtable_insert2(ref->owner->references.loose_cache, ref->name, ref, (void **) &old_ref);

	/* If we force-replaced, we need to free the old reference */
	if(old_ref)
		reference_free(old_ref);

	free(old_name);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to rename reference");

cleanup:
	/* restore the old name if this failed */
	free(ref->name);
	ref->name = old_name;
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to rename reference");

rename_loose_to_old_name:
	/* If we hit this point. Something *bad* happened! Think "Ghostbusters
	 * crossing the streams" definition of bad.
	 * Either the packed-refs has been correctly generated and something else
	 * has gone wrong, or the writing of the new packed-refs has failed, and
	 * we're stuck with the old one. As a loose ref always takes priority over
	 * a packed ref, we'll eventually try and rename the generated loose ref to
	 * its former name. It even that fails, well... we might have lost the reference
	 * for good. :-/
	*/

	git_path_join(old_path, ref->owner->path_repository, ref->name);
	git_path_join(new_path, ref->owner->path_repository, old_name);

	/* No error checking. We'll return the initial error */
	git_futils_mv_withpath(old_path, new_path);

	/* restore the old name */
	free(ref->name);
	ref->name = old_name;

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to rename reference");
}


/*
 * Delete a reference.
 *
 * If the reference is packed, this is an expensive
 * operation. We need to remove the reference from
 * the memory cache and then rewrite the whole pack
 *
 * If the reference is loose, we remove it on
 * the filesystem and update the in-memory cache
 * accordingly. We also make sure that an older version
 * of it doesn't exist as a packed reference. If this
 * is the case, this packed reference is removed as well.
 *
 * This obviously invalidates the `ref` pointer.
 */
int git_reference_delete(git_reference *ref)
{
	int error;
	git_reference *reference;

	assert(ref);

	if (ref->type & GIT_REF_PACKED) {
		/* load the existing packfile */
		if ((error = packed_load(ref->owner)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to delete reference");

		if (git_hashtable_remove(ref->owner->references.packfile, ref->name) < GIT_SUCCESS)
			return git__throw(GIT_ENOTFOUND, "Reference not found");

		error = packed_write(ref->owner);
	} else {
		char full_path[GIT_PATH_MAX];
		git_path_join(full_path, ref->owner->path_repository, ref->name);
		git_hashtable_remove(ref->owner->references.loose_cache, ref->name);
		error = p_unlink(full_path);
		if (error < GIT_SUCCESS)
			goto cleanup;

		/* When deleting a loose reference, we have to ensure that an older
		 * packed version of it doesn't exist
		 */
		if (!git_reference_lookup(&reference, ref->owner, ref->name)) {
			assert((reference->type & GIT_REF_PACKED) != 0);
			error = git_reference_delete(reference);
		}
	}

cleanup:
	reference_free(ref);
	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to delete reference");
}

int git_reference_resolve(git_reference **resolved_ref, git_reference *ref)
{
	git_repository *repo;
	int error, i;

	assert(resolved_ref && ref);
	*resolved_ref = NULL;

	if ((error = loose_update(ref)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to resolve reference");

	repo = ref->owner;

	for (i = 0; i < MAX_NESTING_LEVEL; ++i) {
		reference_symbolic *ref_sym;

		*resolved_ref = ref;

		if (ref->type & GIT_REF_OID)
			return GIT_SUCCESS;

		ref_sym = (reference_symbolic *)ref;
		if ((error = git_reference_lookup(&ref, repo, ref_sym->target)) < GIT_SUCCESS)
			return error;
	}

	return git__throw(GIT_ENOMEM, "Failed to resolve reference. Reference is too nested");
}

int git_reference_packall(git_repository *repo)
{
	int error;

	/* load the existing packfile */
	if ((error = packed_load(repo)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to pack references");

	/* update it in-memory with all the loose references */
	if ((error = packed_loadloose(repo)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to pack references");

	/* write it back to disk */
	return packed_write(repo);
}

int git_reference_foreach(git_repository *repo, unsigned int list_flags, int (*callback)(const char *, void *), void *payload)
{
	int error;
	struct dirent_list_data data;
	char refs_path[GIT_PATH_MAX];

	/* list all the packed references first */
	if (list_flags & GIT_REF_PACKED) {
		const char *ref_name;
		void *GIT_UNUSED(_unused);

		if ((error = packed_load(repo)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to list references");

		GIT_HASHTABLE_FOREACH(repo->references.packfile, ref_name, _unused,
			if ((error = callback(ref_name, payload)) < GIT_SUCCESS)
				return git__throw(error, "Failed to list references. User callback failed");
		);
	}

	/* now list the loose references, trying not to
	 * duplicate the ref names already in the packed-refs file */

	data.repo_path_len = strlen(repo->path_repository);
	data.list_flags = list_flags;
	data.repo = repo;
	data.callback = callback;
	data.callback_payload = payload;


	git_path_join(refs_path, repo->path_repository, GIT_REFS_DIR);
	return git_futils_direach(refs_path, GIT_PATH_MAX, _dirent_loose_listall, &data);
}

int cb__reflist_add(const char *ref, void *data)
{
	return git_vector_insert((git_vector *)data, git__strdup(ref));
}

int git_reference_listall(git_strarray *array, git_repository *repo, unsigned int list_flags)
{
	int error;
	git_vector ref_list;

	assert(array && repo);

	array->strings = NULL;
	array->count = 0;

	if (git_vector_init(&ref_list, 8, NULL) < GIT_SUCCESS)
		return GIT_ENOMEM;

	error = git_reference_foreach(repo, list_flags, &cb__reflist_add, (void *)&ref_list);

	if (error < GIT_SUCCESS) {
		git_vector_free(&ref_list);
		return error;
	}

	array->strings = (char **)ref_list.contents;
	array->count = ref_list.length;
	return GIT_SUCCESS;
}




/*****************************************
 * Init/free (repository API)
 *****************************************/
int git_repository__refcache_init(git_refcache *refs)
{
	assert(refs);

	refs->loose_cache = git_hashtable_alloc(
		default_table_size,
		reftable_hash,
		(git_hash_keyeq_ptr)strcmp);

	/* packfile loaded lazily */
	refs->packfile = NULL;

	return (refs->loose_cache) ? GIT_SUCCESS : GIT_ENOMEM;
}

void git_repository__refcache_free(git_refcache *refs)
{
	git_reference *reference;
	const void *GIT_UNUSED(_unused);

	assert(refs);

	GIT_HASHTABLE_FOREACH(refs->loose_cache, _unused, reference,
		reference_free(reference);
	);

	git_hashtable_free(refs->loose_cache);

	if (refs->packfile) {
		GIT_HASHTABLE_FOREACH(refs->packfile, _unused, reference,
			reference_free(reference);
		);

		git_hashtable_free(refs->packfile);
	}
}



/*****************************************
 * Name normalization
 *****************************************/
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

static int normalize_name(char *buffer_out, size_t out_size, const char *name, int is_oid_ref)
{
	const char *name_end, *buffer_out_start;
	char *current;
	int contains_a_slash = 0;

	assert(name && buffer_out);

	buffer_out_start = buffer_out;
	current = (char *)name;
	name_end = name + strlen(name);

	/* Terminating null byte */
	out_size--;

	/* A refname can not be empty */
	if (name_end == name)
		return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name is empty");

	/* A refname can not end with a dot or a slash */
	if (*(name_end - 1) == '.' || *(name_end - 1) == '/')
		return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name ends with dot or slash");

	while (current < name_end && out_size) {
		if (check_valid_ref_char(*current))
			return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name contains invalid characters");

		if (buffer_out > buffer_out_start) {
			char prev = *(buffer_out - 1);

			/* A refname can not start with a dot nor contain a double dot */
			if (*current == '.' && ((prev == '.') || (prev == '/')))
				return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name starts with a dot or contains a double dot");

			/* '@{' is forbidden within a refname */
			if (*current == '{' && prev == '@')
				return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name contains '@{'");

			/* Prevent multiple slashes from being added to the output */
			if (*current == '/' && prev == '/') {
				current++;
				continue;
			}
		}

		if (*current == '/')
			contains_a_slash = 1;

		*buffer_out++ = *current++;
		out_size--;
	}

	if (!out_size)
		return git__throw(GIT_EINVALIDREFNAME, "Reference name is too long");

	/* Object id refname have to contain at least one slash, except
	 * for HEAD in a detached state or MERGE_HEAD if we're in the
	 * middle of a merge */
	if (is_oid_ref && !contains_a_slash && (strcmp(name, GIT_HEAD_FILE) && strcmp(name, GIT_MERGE_HEAD_FILE)))
		return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name contains no slashes");

	/* A refname can not end with ".lock" */
	if (!git__suffixcmp(name, GIT_FILELOCK_EXTENSION))
		return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name ends with '.lock'");

	*buffer_out = '\0';

	/*
	 * For object id references, name has to start with refs/. Again,
	 * we need to allow HEAD to be in a detached state.
	 */
	if (is_oid_ref && !(git__prefixcmp(buffer_out_start, GIT_REFS_DIR) ||
		strcmp(buffer_out_start, GIT_HEAD_FILE)))
		return git__throw(GIT_EINVALIDREFNAME, "Failed to normalize name. Reference name does not start with 'refs/'");

	return GIT_SUCCESS;
}

int git_reference__normalize_name(char *buffer_out, size_t out_size, const char *name)
{
	return normalize_name(buffer_out, out_size, name, 0);
}

int git_reference__normalize_name_oid(char *buffer_out, size_t out_size, const char *name)
{
	return normalize_name(buffer_out, out_size, name, 1);
}


