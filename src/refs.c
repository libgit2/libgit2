/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "refs.h"
#include "hash.h"
#include "repository.h"
#include "fileops.h"
#include "pack.h"

#include <git2/tag.h>
#include <git2/object.h>

#define MAX_NESTING_LEVEL 5

enum {
	GIT_PACKREF_HAS_PEEL = 1,
	GIT_PACKREF_WAS_LOOSE = 2
};

struct packref {
	git_oid oid;
	git_oid peel;
	char flags;
	char name[GIT_FLEX_ARRAY];
};

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

static int reference_read(
	git_fbuffer *file_content,
	time_t *mtime,
	const char *repo_path,
	const char *ref_name,
	int *updated);

/* loose refs */
static int loose_parse_symbolic(git_reference *ref, git_fbuffer *file_content);
static int loose_parse_oid(git_oid *ref, git_fbuffer *file_content);
static int loose_lookup(git_reference *ref);
static int loose_lookup_to_packfile(struct packref **ref_out,
	git_repository *repo, const char *name);
static int loose_write(git_reference *ref);

/* packed refs */
static int packed_parse_peel(struct packref *tag_ref,
	const char **buffer_out, const char *buffer_end);
static int packed_parse_oid(struct packref **ref_out,
	const char **buffer_out, const char *buffer_end);
static int packed_load(git_repository *repo);
static int packed_loadloose(git_repository *repository);
static int packed_write_ref(struct packref *ref, git_filebuf *file);
static int packed_find_peel(git_repository *repo, struct packref *ref);
static int packed_remove_loose(git_repository *repo, git_vector *packing_list);
static int packed_sort(const void *a, const void *b);
static int packed_lookup(git_reference *ref);
static int packed_write(git_repository *repo);

/* internal helpers */
static int reference_available(git_repository *repo,
	const char *ref, const char *old_ref);
static int reference_delete(git_reference *ref);
static int reference_lookup(git_reference *ref);

/* name normalization */
static int normalize_name(char *buffer_out, size_t out_size,
	const char *name, int is_oid_ref);


void git_reference_free(git_reference *reference)
{
	if (reference == NULL)
		return;

	git__free(reference->name);

	if (reference->flags & GIT_REF_SYMBOLIC)
		git__free(reference->target.symbolic);

	git__free(reference);
}

static int reference_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name)
{
	git_reference *reference = NULL;

	assert(ref_out && repo && name);

	reference = git__malloc(sizeof(git_reference));
	if (reference == NULL)
		return GIT_ENOMEM;

	memset(reference, 0x0, sizeof(git_reference));
	reference->owner = repo;

	reference->name = git__strdup(name);
	if (reference->name == NULL) {
		free(reference);
		return GIT_ENOMEM;
	}

	*ref_out = reference;
	return GIT_SUCCESS;
}

static int reference_read(git_fbuffer *file_content, time_t *mtime, const char *repo_path, const char *ref_name, int *updated)
{
	char path[GIT_PATH_MAX];

	assert(file_content && repo_path && ref_name);

	/* Determine the full path of the file */
	git_path_join(path, repo_path, ref_name);

	return git_futils_readbuffer_updated(file_content, path, mtime, updated);
}

static int loose_parse_symbolic(git_reference *ref, git_fbuffer *file_content)
{
	const unsigned int header_len = strlen(GIT_SYMREF);
	const char *refname_start;
	char *eol;

	refname_start = (const char *)file_content->data;

	if (file_content->len < (header_len + 1))
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Object too short");

	/*
	 * Assume we have already checked for the header
	 * before calling this function
	 */

	refname_start += header_len;

	ref->target.symbolic = git__strdup(refname_start);
	if (ref->target.symbolic == NULL)
		return GIT_ENOMEM;

	/* remove newline at the end of file */
	eol = strchr(ref->target.symbolic, '\n');
	if (eol == NULL)
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Missing EOL");

	*eol = '\0';
	if (eol[-1] == '\r')
		eol[-1] = '\0';

	return GIT_SUCCESS;
}

static int loose_parse_oid(git_oid *oid, git_fbuffer *file_content)
{
	int error;
	char *buffer;

	buffer = (char *)file_content->data;

	/* File format: 40 chars (OID) + newline */
	if (file_content->len < GIT_OID_HEXSZ + 1)
		return git__throw(GIT_EOBJCORRUPTED,
			"Failed to parse loose reference. Reference too short");

	if ((error = git_oid_fromstr(oid, buffer)) < GIT_SUCCESS)
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

static int loose_lookup(git_reference *ref)
{
	int error = GIT_SUCCESS, updated;
	git_fbuffer ref_file = GIT_FBUFFER_INIT;

	if (reference_read(&ref_file, &ref->mtime,
			ref->owner->path_repository, ref->name, &updated) < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "Failed to lookup loose reference");

	if (!updated)
		return GIT_SUCCESS;

	if (ref->flags & GIT_REF_SYMBOLIC)
		free(ref->target.symbolic);

	ref->flags = 0;

	if (git__prefixcmp((const char *)(ref_file.data), GIT_SYMREF) == 0) {
		ref->flags |= GIT_REF_SYMBOLIC;
		error = loose_parse_symbolic(ref, &ref_file);
	} else {
		ref->flags |= GIT_REF_OID;
		error = loose_parse_oid(&ref->target.oid, &ref_file);
	}

	git_futils_freebuffer(&ref_file);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup loose reference");

	return GIT_SUCCESS;
}

static int loose_lookup_to_packfile(
		struct packref **ref_out,
		git_repository *repo,
		const char *name)
{
	int error = GIT_SUCCESS;
	git_fbuffer ref_file = GIT_FBUFFER_INIT;
	struct packref *ref = NULL;
	size_t name_len;

	*ref_out = NULL;

	error = reference_read(&ref_file, NULL, repo->path_repository, name, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	name_len = strlen(name);
	ref = git__malloc(sizeof(struct packref) + name_len + 1);

	memcpy(ref->name, name, name_len);
	ref->name[name_len] = 0;

	error = loose_parse_oid(&ref->oid, &ref_file);
	if (error < GIT_SUCCESS)
		goto cleanup;

	ref->flags = GIT_PACKREF_WAS_LOOSE;

	*ref_out = ref;
	git_futils_freebuffer(&ref_file);
	return GIT_SUCCESS;

cleanup:
	git_futils_freebuffer(&ref_file);
	free(ref);
	return git__rethrow(error, "Failed to lookup loose reference");
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

	if (ref->flags & GIT_REF_OID) {
		char oid[GIT_OID_HEXSZ + 1];

		git_oid_fmt(oid, &ref->target.oid);
		oid[GIT_OID_HEXSZ] = '\0';

		error = git_filebuf_printf(&file, "%s\n", oid);
		if (error < GIT_SUCCESS)
			goto unlock;

	} else if (ref->flags & GIT_REF_SYMBOLIC) { /* GIT_REF_SYMBOLIC */
		error = git_filebuf_printf(&file, GIT_SYMREF "%s\n", ref->target.symbolic);
	} else {
		error = git__throw(GIT_EOBJCORRUPTED,
			"Failed to write reference. Invalid reference type");
		goto unlock;
	}

	if (p_stat(ref_path, &st) == GIT_SUCCESS)
		ref->mtime = st.st_mtime;

	return git_filebuf_commit(&file, GIT_REFS_FILE_MODE);

unlock:
	git_filebuf_cleanup(&file);
	return git__rethrow(error, "Failed to write loose reference");
}

static int packed_parse_peel(
		struct packref *tag_ref,
		const char **buffer_out,
		const char *buffer_end)
{
	const char *buffer = *buffer_out + 1;

	assert(buffer[-1] == '^');

	/* Ensure it's not the first entry of the file */
	if (tag_ref == NULL)
		return git__throw(GIT_EPACKEDREFSCORRUPTED,
			"Failed to parse packed reference. "
			"Reference is the first entry of the file");

	/* Ensure reference is a tag */
	if (git__prefixcmp(tag_ref->name, GIT_REFS_TAGS_DIR) != 0)
		return git__throw(GIT_EPACKEDREFSCORRUPTED,
			"Failed to parse packed reference. Reference is not a tag");

	if (buffer + GIT_OID_HEXSZ >= buffer_end)
		return git__throw(GIT_EPACKEDREFSCORRUPTED,
			"Failed to parse packed reference. Buffer too small");

	/* Is this a valid object id? */
	if (git_oid_fromstr(&tag_ref->peel, buffer) < GIT_SUCCESS)
		return git__throw(GIT_EPACKEDREFSCORRUPTED,
			"Failed to parse packed reference. Not a valid object ID");

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (*buffer != '\n')
		return git__throw(GIT_EPACKEDREFSCORRUPTED,
			"Failed to parse packed reference. Buffer not terminated correctly");

	*buffer_out = buffer + 1;
	return GIT_SUCCESS;
}

static int packed_parse_oid(
		struct packref **ref_out,
		const char **buffer_out,
		const char *buffer_end)
{
	struct packref *ref = NULL;

	const char *buffer = *buffer_out;
	const char *refname_begin, *refname_end;

	int error = GIT_SUCCESS;
	size_t refname_len;
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

	if (refname_end[-1] == '\r')
		refname_end--;

	refname_len = refname_end - refname_begin;

	ref = git__malloc(sizeof(struct packref) + refname_len + 1);

	memcpy(ref->name, refname_begin, refname_len);
	ref->name[refname_len] = 0;

	git_oid_cpy(&ref->oid, &id);

	ref->flags = 0;

	*ref_out = ref;
	*buffer_out = refname_end + 1;

	return GIT_SUCCESS;

cleanup:
	free(ref);
	return git__rethrow(error, "Failed to parse OID of packed reference");
}

static int packed_load(git_repository *repo)
{
	int error = GIT_SUCCESS, updated;
	git_fbuffer packfile = GIT_FBUFFER_INIT;
	const char *buffer_start, *buffer_end;
	git_refcache *ref_cache = &repo->references;

	/* First we make sure we have allocated the hash table */
	if (ref_cache->packfile == NULL) {
		ref_cache->packfile = git_hashtable_alloc(
			default_table_size,
			reftable_hash,
			(git_hash_keyeq_ptr)&git__strcmp_cb);

		if (ref_cache->packfile == NULL) {
			error = GIT_ENOMEM;
			goto cleanup;
		}
	}

	error = reference_read(&packfile, &ref_cache->packfile_time,
		repo->path_repository, GIT_PACKEDREFS_FILE, &updated);

	/*
	 * If we couldn't find the file, we need to clear the table and
	 * return. On any other error, we return that error. If everything
	 * went fine and the file wasn't updated, then there's nothing new
	 * for us here, so just return. Anything else means we need to
	 * refresh the packed refs.
	 */
	if (error == GIT_ENOTFOUND) {
		git_hashtable_clear(ref_cache->packfile);
		return GIT_SUCCESS;
	} else if (error < GIT_SUCCESS) {
		return git__rethrow(error, "Failed to read packed refs");
	} else if (!updated) {
		return GIT_SUCCESS;
	}

	/*
	 * At this point, we want to refresh the packed refs. We already
	 * have the contents in our buffer.
	 */

	git_hashtable_clear(ref_cache->packfile);

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
		struct packref *ref = NULL;

		error = packed_parse_oid(&ref, &buffer_start, buffer_end);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if (buffer_start[0] == '^') {
			error = packed_parse_peel(ref, &buffer_start, buffer_end);
			if (error < GIT_SUCCESS)
				goto cleanup;
		}

		error = git_hashtable_insert(ref_cache->packfile, ref->name, ref);
		if (error < GIT_SUCCESS) {
			free(ref);
			goto cleanup;
		}
	}

	git_futils_freebuffer(&packfile);
	return GIT_SUCCESS;

cleanup:
	git_hashtable_free(ref_cache->packfile);
	ref_cache->packfile = NULL;
	git_futils_freebuffer(&packfile);
	return git__rethrow(error, "Failed to load packed references");
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
		return git_futils_direach(full_path, GIT_PATH_MAX,
			_dirent_loose_listall, _data);

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
	void *old_ref = NULL;
	struct packref *ref;
	char *file_path;
	int error;

	if (git_futils_isdir(full_path) == GIT_SUCCESS)
		return git_futils_direach(
			full_path, GIT_PATH_MAX,
			_dirent_loose_load, repository);

	file_path = full_path + strlen(repository->path_repository);
	error = loose_lookup_to_packfile(&ref, repository, file_path);

	if (error == GIT_SUCCESS) {

		if (git_hashtable_insert2(
			repository->references.packfile,
			ref->name, ref, &old_ref) < GIT_SUCCESS) {
			free(ref);
			return GIT_ENOMEM;
		}

		if (old_ref != NULL)
			free(old_ref);
	}

	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to load loose references into packfile");
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

	/*
	 * Load all the loose files from disk into the Packfile table.
	 * This will overwrite any old packed entries with their
	 * updated loose versions
	 */
	return git_futils_direach(
		refs_path, GIT_PATH_MAX,
		_dirent_loose_load, repository);
}

/*
 * Write a single reference into a packfile
 */
static int packed_write_ref(struct packref *ref, git_filebuf *file)
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
	if (ref->flags & GIT_PACKREF_HAS_PEEL) {
		char peel[GIT_OID_HEXSZ + 1];
		git_oid_fmt(peel, &ref->peel);
		peel[GIT_OID_HEXSZ] = 0;

		error = git_filebuf_printf(file, "%s %s\n^%s\n", oid, ref->name, peel);
	} else {
		error = git_filebuf_printf(file, "%s %s\n", oid, ref->name);
	}

	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to write packed reference");
}

/*
 * Find out what object this reference resolves to.
 *
 * For references that point to a 'big' tag (e.g. an
 * actual tag object on the repository), we need to
 * cache on the packfile the OID of the object to
 * which that 'big tag' is pointing to.
 */
static int packed_find_peel(git_repository *repo, struct packref *ref)
{
	git_object *object;
	int error;

	if (ref->flags & GIT_PACKREF_HAS_PEEL)
		return GIT_SUCCESS;

	/*
	 * Only applies to tags, i.e. references
	 * in the /refs/tags folder
	 */
	if (git__prefixcmp(ref->name, GIT_REFS_TAGS_DIR) != 0)
		return GIT_SUCCESS;

	/*
	 * Find the tagged object in the repository
	 */
	error = git_object_lookup(&object, repo, &ref->oid, GIT_OBJ_ANY);
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
		git_oid_cpy(&ref->peel, git_tag_target_oid(tag));
		ref->flags |= GIT_PACKREF_HAS_PEEL;

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

	for (i = 0; i < packing_list->length; ++i) {
		struct packref *ref = git_vector_get(packing_list, i);

		if ((ref->flags & GIT_PACKREF_WAS_LOOSE) == 0)
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
		 */
	}

	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to remove loose packed reference");
}

static int packed_sort(const void *a, const void *b)
{
	const struct packref *ref_a = (const struct packref *)a;
	const struct packref *ref_b = (const struct packref *)b;

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
	if ((error =
		git_vector_init(&packing_list, total_refs, packed_sort)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to init packed refernces list");

	/* Load all the packfile into a vector */
	{
		struct packref *reference;
		const void *GIT_UNUSED(_unused);

		GIT_HASHTABLE_FOREACH(repo->references.packfile, _unused, reference,
			/* cannot fail: vector already has the right size */
			git_vector_insert(&packing_list, reference);
		);
	}

	/* sort the vector so the entries appear sorted on the packfile */
	git_vector_sort(&packing_list);

	/* Now we can open the file! */
	git_path_join(pack_file_path, repo->path_repository, GIT_PACKEDREFS_FILE);
	if ((error = git_filebuf_open(&pack_file, pack_file_path, 0)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write open packed references file");

	/* Packfiles have a header... apparently
	 * This is in fact not required, but we might as well print it
	 * just for kicks */
	if ((error =
		git_filebuf_printf(&pack_file, "%s\n", GIT_PACKEDREFS_HEADER)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write packed references file header");

	for (i = 0; i < packing_list.length; ++i) {
		struct packref *ref = (struct packref *)git_vector_get(&packing_list, i);

		if ((error = packed_find_peel(repo, ref)) < GIT_SUCCESS) {
			error = git__throw(GIT_EOBJCORRUPTED,
				"A reference cannot be peeled");
			goto cleanup;
		}

		if ((error = packed_write_ref(ref, &pack_file)) < GIT_SUCCESS)
			goto cleanup;
	}

cleanup:
	/* if we've written all the references properly, we can commit
	 * the packfile to make the changes effective */
	if (error == GIT_SUCCESS) {
		error = git_filebuf_commit(&pack_file, GIT_PACKEDREFS_FILE_MODE);

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

	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to write packed references file");
}

static int _reference_available_cb(const char *ref, void *data)
{
	const char *new, *old;
	const char **refs;

	assert(ref && data);

	refs = (const char **)data;

	new = (const char *)refs[0];
	old = (const char *)refs[1];

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

static int reference_available(
	git_repository *repo,
	const char *ref,
	const char* old_ref)
{
	const char *refs[2];

	refs[0] = ref;
	refs[1] = old_ref;

	if (git_reference_foreach(repo, GIT_REF_LISTALL,
		_reference_available_cb, (void *)refs) < 0) {
		return git__throw(GIT_EEXISTS,
			"Reference name `%s` conflicts with existing reference", ref);
	}

	return GIT_SUCCESS;
}

static int reference_exists(int *exists, git_repository *repo, const char *ref_name)
{
	int error;
	char ref_path[GIT_PATH_MAX];

	error = packed_load(repo);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Cannot resolve if a reference exists");

	git_path_join(ref_path, repo->path_repository, ref_name);

	if (git_futils_isfile(ref_path) == GIT_SUCCESS ||
		git_hashtable_lookup(repo->references.packfile, ref_path) != NULL) {
		*exists = 1;
	} else {
		*exists = 0;
	}

	return GIT_SUCCESS;
}

static int packed_lookup(git_reference *ref)
{
	int error;
	struct packref *pack_ref = NULL;

	error = packed_load(ref->owner);
	if (error < GIT_SUCCESS)
		return git__rethrow(error,
			"Failed to lookup reference from packfile");

	if (ref->flags & GIT_REF_PACKED &&
		ref->mtime == ref->owner->references.packfile_time)
		return GIT_SUCCESS;

	if (ref->flags & GIT_REF_SYMBOLIC)
		free(ref->target.symbolic);

	/* Look up on the packfile */
	pack_ref = git_hashtable_lookup(ref->owner->references.packfile, ref->name);
	if (pack_ref == NULL)
		return git__throw(GIT_ENOTFOUND,
			"Failed to lookup reference from packfile");

	ref->flags = GIT_REF_OID | GIT_REF_PACKED;
	ref->mtime = ref->owner->references.packfile_time;
	git_oid_cpy(&ref->target.oid, &pack_ref->oid);

	return GIT_SUCCESS;
}

static int reference_lookup(git_reference *ref)
{
	int error_loose, error_packed;

	error_loose = loose_lookup(ref);
	if (error_loose == GIT_SUCCESS)
		return GIT_SUCCESS;

	error_packed = packed_lookup(ref);
	if (error_packed == GIT_SUCCESS)
		return GIT_SUCCESS;

	git_reference_free(ref);

	if (error_loose != GIT_ENOTFOUND)
		return git__rethrow(error_loose, "Failed to lookup reference");

	if (error_packed != GIT_ENOTFOUND)
		return git__rethrow(error_packed, "Failed to lookup reference");

	return git__throw(GIT_ENOTFOUND, "Reference not found");
}

/*
 * Delete a reference.
 * This is an internal method; the reference is removed
 * from disk or the packfile, but the pointer is not freed
 */
static int reference_delete(git_reference *ref)
{
	int error;

	assert(ref);

	/* If the reference is packed, this is an expensive operation.
	 * We need to reload the packfile, remove the reference from the
	 * packing list, and repack */
	if (ref->flags & GIT_REF_PACKED) {
		/* load the existing packfile */
		if ((error = packed_load(ref->owner)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to delete reference");

		if (git_hashtable_remove(ref->owner->references.packfile,
				ref->name) < GIT_SUCCESS)
			return git__throw(GIT_ENOTFOUND, "Reference not found");

		error = packed_write(ref->owner);

	/* If the reference is loose, we can just remove the reference
	 * from the filesystem */
	} else {
		char full_path[GIT_PATH_MAX];
		git_reference *ref_in_pack;

		git_path_join(full_path, ref->owner->path_repository, ref->name);

		error = p_unlink(full_path);
		if (error < GIT_SUCCESS)
			goto cleanup;

		/* When deleting a loose reference, we have to ensure that an older
		 * packed version of it doesn't exist */
		if (git_reference_lookup(&ref_in_pack, ref->owner,
				ref->name) == GIT_SUCCESS) {
			assert((ref_in_pack->flags & GIT_REF_PACKED) != 0);
			error = git_reference_delete(ref_in_pack);
		}
	}

cleanup:
	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to delete reference");
}

int git_reference_delete(git_reference *ref)
{
	int error = reference_delete(ref);
	if (error < GIT_SUCCESS)
		return error;

	git_reference_free(ref);
	return GIT_SUCCESS;
}


int git_reference_lookup(git_reference **ref_out,
	git_repository *repo, const char *name)
{
	int error;
	char normalized_name[GIT_REFNAME_MAX];
	git_reference *ref = NULL;

	assert(ref_out && repo && name);

	*ref_out = NULL;

	error = normalize_name(normalized_name, sizeof(normalized_name), name, 0);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference");

	error = reference_create(&ref, repo, normalized_name);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference");

	error = reference_lookup(ref);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference");

	*ref_out = ref;
	return GIT_SUCCESS;
}

/**
 * Getters
 */
git_rtype git_reference_type(git_reference *ref)
{
	assert(ref);

	if (ref->flags & GIT_REF_OID)
		return GIT_REF_OID;

	if (ref->flags & GIT_REF_SYMBOLIC)
		return GIT_REF_SYMBOLIC;

	return GIT_REF_INVALID;
}

int git_reference_is_packed(git_reference *ref)
{
	assert(ref);
	return !!(ref->flags & GIT_REF_PACKED);
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

	if ((ref->flags & GIT_REF_OID) == 0)
		return NULL;

	return &ref->target.oid;
}

const char *git_reference_target(git_reference *ref)
{
	assert(ref);

	if ((ref->flags & GIT_REF_SYMBOLIC) == 0)
		return NULL;

	return ref->target.symbolic;
}

int git_reference_create_symbolic(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const char *target,
	int force)
{
	char normalized[GIT_REFNAME_MAX];
	int ref_exists, error = GIT_SUCCESS;
	git_reference *ref = NULL;

	error = normalize_name(normalized, sizeof(normalized), name, 0);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if ((error = reference_exists(&ref_exists, repo, normalized) < GIT_SUCCESS))
		return git__rethrow(error, "Failed to create symbolic reference");

	if (ref_exists && !force)
		return git__throw(GIT_EEXISTS,
			"Failed to create symbolic reference. Reference already exists");

	error = reference_create(&ref, repo, normalized);
	if (error < GIT_SUCCESS)
		goto cleanup;

	ref->flags |= GIT_REF_SYMBOLIC;

	/* set the target; this will normalize the name automatically
	 * and write the reference on disk */
	error = git_reference_set_target(ref, target);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (ref_out == NULL) {
		git_reference_free(ref);
	} else {
		*ref_out = ref;
	}

	return GIT_SUCCESS;

cleanup:
	git_reference_free(ref);
	return git__rethrow(error, "Failed to create symbolic reference");
}

int git_reference_create_oid(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const git_oid *id,
	int force)
{
	int error = GIT_SUCCESS, ref_exists;
	git_reference *ref = NULL;
	char normalized[GIT_REFNAME_MAX];

	error = normalize_name(normalized, sizeof(normalized), name, 1);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if ((error = reference_exists(&ref_exists, repo, normalized) < GIT_SUCCESS))
		return git__rethrow(error, "Failed to create OID reference");

	if (ref_exists && !force)
		return git__throw(GIT_EEXISTS,
			"Failed to create OID reference. Reference already exists");

	if ((error = reference_available(repo, name, NULL)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create reference");

	error = reference_create(&ref, repo, name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	ref->flags |= GIT_REF_OID;

	/* set the oid; this will write the reference on disk */
	error = git_reference_set_oid(ref, id);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (ref_out == NULL) {
		git_reference_free(ref);
	} else {
		*ref_out = ref;
	}

	return GIT_SUCCESS;

cleanup:
	git_reference_free(ref);
	return git__rethrow(error, "Failed to create reference OID");
}

/*
 * Change the OID target of a reference.
 *
 * For both loose and packed references, just change
 * the oid in memory and (over)write the file in disk.
 *
 * We do not repack packed references because of performance
 * reasons.
 */
int git_reference_set_oid(git_reference *ref, const git_oid *id)
{
	int error = GIT_SUCCESS;

	if ((ref->flags & GIT_REF_OID) == 0)
		return git__throw(GIT_EINVALIDREFSTATE,
			"Failed to set OID target of reference. Not an OID reference");

	assert(ref->owner);

	/* Don't let the user create references to OIDs that
	 * don't exist in the ODB */
	if (!git_odb_exists(git_repository_database(ref->owner), id))
		return git__throw(GIT_ENOTFOUND,
			"Failed to set OID target of reference. OID doesn't exist in ODB");

	/* Update the OID value on `ref` */
	git_oid_cpy(&ref->target.oid, id);

	error = loose_write(ref);
	if (error < GIT_SUCCESS)
		goto cleanup;

	return GIT_SUCCESS;

cleanup:
	return git__rethrow(error, "Failed to set OID target of reference");
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
	int error;
	char normalized[GIT_REFNAME_MAX];

	if ((ref->flags & GIT_REF_SYMBOLIC) == 0)
		return git__throw(GIT_EINVALIDREFSTATE,
			"Failed to set reference target. Not a symbolic reference");

	error = normalize_name(normalized, sizeof(normalized), target, 0);
	if (error < GIT_SUCCESS)
		return git__rethrow(error,
			"Failed to set reference target. Invalid target name");

	git__free(ref->target.symbolic);
	ref->target.symbolic = git__strdup(normalized);
	if (ref->target.symbolic == NULL)
		return GIT_ENOMEM;

	return loose_write(ref);
}

int git_reference_rename(git_reference *ref, const char *new_name, int force)
{
	int error;

	char aux_path[GIT_PATH_MAX];
	char normalized[GIT_REFNAME_MAX];

	const char *head_target = NULL;
	git_reference *existing_ref = NULL, *head = NULL;

	error = normalize_name(normalized, sizeof(normalized),
		new_name, ref->flags & GIT_REF_OID);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to rename reference. Invalid name");

	new_name = normalized;

	/* If we are forcing the rename, try to lookup a reference with the
	 * new one. If the lookup succeeds, we need to delete that ref
	 * before the renaming can proceed */
	if (force) {
		error = git_reference_lookup(&existing_ref, ref->owner, new_name);

		if (error == GIT_SUCCESS) {
			error = git_reference_delete(existing_ref);
			if (error < GIT_SUCCESS)
				return git__rethrow(error,
					"Failed to rename reference. "
					"The existing reference cannot be deleted");
		} else if (error != GIT_ENOTFOUND)
			goto cleanup;

	/* If we're not forcing the rename, check if the reference exists.
	 * If it does, renaming cannot continue */
	} else {
		int exists;

		error = reference_exists(&exists, ref->owner, normalized);
		if (error < GIT_SUCCESS)
			goto cleanup;

		if (exists)
			return git__throw(GIT_EEXISTS,
				"Failed to rename reference. Reference already exists");
	}

	if ((error = reference_available(ref->owner, new_name, ref->name)) < GIT_SUCCESS)
		return git__rethrow(error,
			"Failed to rename reference. Reference already exists");

	/*
	 * Now delete the old ref and remove an possibly existing directory
	 * named `new_name`. Note that using the internal `reference_delete`
	 * method deletes the ref from disk but doesn't free the pointer, so
	 * we can still access the ref's attributes for creating the new one
	 */
	if ((error = reference_delete(ref)) < GIT_SUCCESS)
		goto cleanup;

	git_path_join(aux_path, ref->owner->path_repository, new_name);
	if (git_futils_exists(aux_path) == GIT_SUCCESS) {
		if (git_futils_isdir(aux_path) == GIT_SUCCESS) {
			if ((error = git_futils_rmdir_r(aux_path, 0)) < GIT_SUCCESS)
				goto rollback;
		} else goto rollback;
	}

	/*
	 * Crude hack: delete any logs till we support proper reflogs.
	 * Otherwise git.git will possibly fail and leave a mess. git.git
	 * writes reflogs by default in any repo with a working directory:
	 *
	 * "We only enable reflogs in repositories that have a working directory
	 * associated with them, as shared/bare repositories do not have
	 * an easy means to prune away old log entries, or may fail logging
	 * entirely if the user's gecos information is not valid during a push.
	 * This heuristic was suggested on the mailing list by Junio."
	 *
	 * 	Shawn O. Pearce - 0bee59186976b1d9e6b2dd77332480c9480131d5
	 *
	 * TODO
	 *
	 */
	git_path_join_n(aux_path, 3, ref->owner->path_repository, "logs", ref->name);
	if (git_futils_isfile(aux_path) == GIT_SUCCESS) {
		if ((error = p_unlink(aux_path)) < GIT_SUCCESS)
			goto rollback;
	}

	/*
	 * Finally we can create the new reference.
	 */
	if (ref->flags & GIT_REF_SYMBOLIC) {
		error = git_reference_create_symbolic(
			NULL, ref->owner, new_name, ref->target.symbolic, 0);
	} else {
		error = git_reference_create_oid(
			NULL, ref->owner, new_name, &ref->target.oid, 0);
	}

	if (error < GIT_SUCCESS)
		goto cleanup;

	/*
	 * Check if we have to update HEAD.
	 */
	error = git_reference_lookup(&head, ref->owner, GIT_HEAD_FILE);
	if (error < GIT_SUCCESS)
		goto cleanup;

	head_target = git_reference_target(head);

	if (head_target && !strcmp(head_target, ref->name)) {
		error = git_reference_create_symbolic(
			&head, ref->owner, "HEAD", new_name, 1);

		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	/*
	 * Change the name of the reference given by the user.
	 */
	git__free(ref->name);
	ref->name = git__strdup(new_name);

	/* The reference is no longer packed */
	ref->flags &= ~GIT_REF_PACKED;

cleanup:
	/* We no longer need the newly created reference nor the head */
	git_reference_free(head);
	return error == GIT_SUCCESS ?
		GIT_SUCCESS :
		git__rethrow(error, "Failed to rename reference");

rollback:
	/*
	 * Try to create the old reference again.
	 */
	if (ref->flags & GIT_REF_SYMBOLIC)
		error = git_reference_create_symbolic(
			NULL, ref->owner, ref->name, ref->target.symbolic, 0);
	else
		error = git_reference_create_oid(
			NULL, ref->owner, ref->name, &ref->target.oid, 0);

	return error == GIT_SUCCESS ?
		git__rethrow(GIT_ERROR, "Failed to rename reference. Did rollback") :
		git__rethrow(error, "Failed to rename reference. Failed to rollback");
}

int git_reference_resolve(git_reference **ref_out, git_reference *ref)
{
	int error, i = 0;
	git_repository *repo;

	assert(ref);

	*ref_out = NULL;
	repo = ref->owner;

	/* If the reference is already resolved, we need to return a
	 * copy. Instead of duplicating `ref`, we look it up again to
	 * ensure the copy is out to date */
	if (ref->flags & GIT_REF_OID)
		return git_reference_lookup(ref_out, ref->owner, ref->name);

	/* Otherwise, keep iterating until the reference is resolved */
	for (i = 0; i < MAX_NESTING_LEVEL; ++i) {
		git_reference *new_ref;

		error = git_reference_lookup(&new_ref, repo, ref->target.symbolic);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to resolve reference");

		/* Free intermediate references, except for the original one
		 * we've received */
		if (i > 0)
			git_reference_free(ref);

		ref = new_ref;

		/* When the reference we've just looked up is an OID, we've
		 * successfully resolved the symbolic ref */
		if (ref->flags & GIT_REF_OID) {
			*ref_out = ref;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_ENOMEM,
		"Failed to resolve reference. Reference is too nested");
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

int git_reference_foreach(
	git_repository *repo,
	unsigned int list_flags,
	int (*callback)(const char *, void *),
	void *payload)
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
				return git__throw(error,
					"Failed to list references. User callback failed");
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

static int cb__reflist_add(const char *ref, void *data)
{
	return git_vector_insert((git_vector *)data, git__strdup(ref));
}

int git_reference_listall(
	git_strarray *array,
	git_repository *repo,
	unsigned int list_flags)
{
	int error;
	git_vector ref_list;

	assert(array && repo);

	array->strings = NULL;
	array->count = 0;

	if (git_vector_init(&ref_list, 8, NULL) < GIT_SUCCESS)
		return GIT_ENOMEM;

	error = git_reference_foreach(
		repo, list_flags, &cb__reflist_add, (void *)&ref_list);

	if (error < GIT_SUCCESS) {
		git_vector_free(&ref_list);
		return error;
	}

	array->strings = (char **)ref_list.contents;
	array->count = ref_list.length;
	return GIT_SUCCESS;
}

int git_reference_reload(git_reference *ref)
{
	int error = reference_lookup(ref);

	if (error < GIT_SUCCESS) {
		git_reference_free(ref);
		return git__rethrow(error, "Failed to reload reference");
	}

	return GIT_SUCCESS;
}


void git_repository__refcache_free(git_refcache *refs)
{
	assert(refs);

	if (refs->packfile) {
		const void *GIT_UNUSED(_unused);
		struct packref *reference;

		GIT_HASHTABLE_FOREACH(refs->packfile, _unused, reference,
			free(reference);
		);

		git_hashtable_free(refs->packfile);
	}
}

static int is_valid_ref_char(char ch)
{
	if ((unsigned) ch <= ' ')
		return 0;

	switch (ch) {
	case '~':
	case '^':
	case ':':
	case '\\':
	case '?':
	case '[':
	case '*':
		return 0;
	default:
		return 1;
	}
}

static int normalize_name(
	char *buffer_out,
	size_t out_size,
	const char *name,
	int is_oid_ref)
{
	const char *name_end, *buffer_out_start;
	const char *current;
	int contains_a_slash = 0;

	assert(name && buffer_out);

	buffer_out_start = buffer_out;
	current = name;
	name_end = name + strlen(name);

	/* Terminating null byte */
	out_size--;

	/* A refname can not be empty */
	if (name_end == name)
		return git__throw(GIT_EINVALIDREFNAME,
			"Failed to normalize name. Reference name is empty");

	/* A refname can not end with a dot or a slash */
	if (*(name_end - 1) == '.' || *(name_end - 1) == '/')
		return git__throw(GIT_EINVALIDREFNAME,
			"Failed to normalize name. Reference name ends with dot or slash");

	while (current < name_end && out_size) {
		if (!is_valid_ref_char(*current))
			return git__throw(GIT_EINVALIDREFNAME,
				"Failed to normalize name. "
				"Reference name contains invalid characters");

		if (buffer_out > buffer_out_start) {
			char prev = *(buffer_out - 1);

			/* A refname can not start with a dot nor contain a double dot */
			if (*current == '.' && ((prev == '.') || (prev == '/')))
				return git__throw(GIT_EINVALIDREFNAME,
					"Failed to normalize name. "
					"Reference name starts with a dot or contains a double dot");

			/* '@{' is forbidden within a refname */
			if (*current == '{' && prev == '@')
				return git__throw(GIT_EINVALIDREFNAME,
					"Failed to normalize name. Reference name contains '@{'");

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
	if (is_oid_ref &&
		!contains_a_slash &&
		strcmp(name, GIT_HEAD_FILE) != 0 &&
		strcmp(name, GIT_MERGE_HEAD_FILE) != 0 &&
		strcmp(name, GIT_FETCH_HEAD_FILE) != 0)
		return git__throw(GIT_EINVALIDREFNAME,
			"Failed to normalize name. Reference name contains no slashes");

	/* A refname can not end with ".lock" */
	if (!git__suffixcmp(name, GIT_FILELOCK_EXTENSION))
		return git__throw(GIT_EINVALIDREFNAME,
			"Failed to normalize name. Reference name ends with '.lock'");

	*buffer_out = '\0';

	/*
	 * For object id references, name has to start with refs/. Again,
	 * we need to allow HEAD to be in a detached state.
	 */
	if (is_oid_ref && !(git__prefixcmp(buffer_out_start, GIT_REFS_DIR) ||
		strcmp(buffer_out_start, GIT_HEAD_FILE)))
		return git__throw(GIT_EINVALIDREFNAME,
			"Failed to normalize name. "
			"Reference name does not start with 'refs/'");

	return GIT_SUCCESS;
}

int git_reference__normalize_name(
	char *buffer_out,
	size_t out_size,
	const char *name)
{
	return normalize_name(buffer_out, out_size, name, 0);
}

int git_reference__normalize_name_oid(
	char *buffer_out,
	size_t out_size,
	const char *name)
{
	return normalize_name(buffer_out, out_size, name, 1);
}
