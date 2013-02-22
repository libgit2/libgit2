/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "refs.h"
#include "hash.h"
#include "repository.h"
#include "fileops.h"
#include "pack.h"
#include "reflog.h"

#include <git2/tag.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/branch.h>

GIT__USE_STRMAP;

#define DEFAULT_NESTING_LEVEL	5
#define MAX_NESTING_LEVEL		10

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

static int reference_read(
	git_buf *file_content,
	time_t *mtime,
	const char *repo_path,
	const char *ref_name,
	int *updated);

/* loose refs */
static int loose_parse_symbolic(git_reference *ref, git_buf *file_content);
static int loose_parse_oid(git_oid *ref, git_buf *file_content);
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
static int reference_path_available(git_repository *repo,
	const char *ref, const char *old_ref);
static int reference_delete(git_reference *ref);
static int reference_lookup(git_reference *ref);

void git_reference_free(git_reference *reference)
{
	if (reference == NULL)
		return;

	git__free(reference->name);
	reference->name = NULL;

	if (reference->flags & GIT_REF_SYMBOLIC) {
		git__free(reference->target.symbolic);
		reference->target.symbolic = NULL;
	}

	git__free(reference);
}

static int reference_alloc(
	git_reference **ref_out,
	git_repository *repo,
	const char *name)
{
	git_reference *reference = NULL;

	assert(ref_out && repo && name);

	reference = git__malloc(sizeof(git_reference));
	GITERR_CHECK_ALLOC(reference);

	memset(reference, 0x0, sizeof(git_reference));
	reference->owner = repo;

	reference->name = git__strdup(name);
	GITERR_CHECK_ALLOC(reference->name);

	*ref_out = reference;
	return 0;
}

static int reference_read(
	git_buf *file_content,
	time_t *mtime,
	const char *repo_path,
	const char *ref_name,
	int *updated)
{
	git_buf path = GIT_BUF_INIT;
	int result;

	assert(file_content && repo_path && ref_name);

	/* Determine the full path of the file */
	if (git_buf_joinpath(&path, repo_path, ref_name) < 0)
		return -1;

	result = git_futils_readbuffer_updated(
		file_content, path.ptr, mtime, NULL, updated);
	git_buf_free(&path);

	return result;
}

static int loose_parse_symbolic(git_reference *ref, git_buf *file_content)
{
	const unsigned int header_len = (unsigned int)strlen(GIT_SYMREF);
	const char *refname_start;

	refname_start = (const char *)file_content->ptr;

	if (git_buf_len(file_content) < header_len + 1) {
		giterr_set(GITERR_REFERENCE, "Corrupted loose reference file");
		return -1;
	}

	/*
	 * Assume we have already checked for the header
	 * before calling this function
	 */
	refname_start += header_len;

	ref->target.symbolic = git__strdup(refname_start);
	GITERR_CHECK_ALLOC(ref->target.symbolic);

	return 0;
}

static int loose_parse_oid(git_oid *oid, git_buf *file_content)
{
	size_t len;
	const char *str;

	len = git_buf_len(file_content);
	if (len < GIT_OID_HEXSZ)
		goto corrupted;

	/* str is guranteed to be zero-terminated */
	str = git_buf_cstr(file_content);

	/* we need to get 40 OID characters from the file */
	if (git_oid_fromstr(oid, git_buf_cstr(file_content)) < 0)
		goto corrupted;

	/* If the file is longer than 40 chars, the 41st must be a space */
	str += GIT_OID_HEXSZ;
	if (*str == '\0' || git__isspace(*str))
		return 0;

corrupted:
	giterr_set(GITERR_REFERENCE, "Corrupted loose reference file");
	return -1;
}

static git_ref_t loose_guess_rtype(const git_buf *full_path)
{
	git_buf ref_file = GIT_BUF_INIT;
	git_ref_t type;

	type = GIT_REF_INVALID;

	if (git_futils_readbuffer(&ref_file, full_path->ptr) == 0) {
		if (git__prefixcmp((const char *)(ref_file.ptr), GIT_SYMREF) == 0)
			type = GIT_REF_SYMBOLIC;
		else
			type = GIT_REF_OID;
	}

	git_buf_free(&ref_file);
	return type;
}

static int loose_lookup(git_reference *ref)
{
	int result, updated;
	git_buf ref_file = GIT_BUF_INIT;

	result = reference_read(&ref_file, &ref->mtime,
		ref->owner->path_repository, ref->name, &updated);

	if (result < 0)
		return result;

	if (!updated)
		return 0;

	if (ref->flags & GIT_REF_SYMBOLIC) {
		git__free(ref->target.symbolic);
		ref->target.symbolic = NULL;
	}

	ref->flags = 0;

	if (git__prefixcmp((const char *)(ref_file.ptr), GIT_SYMREF) == 0) {
		ref->flags |= GIT_REF_SYMBOLIC;
		git_buf_rtrim(&ref_file);
		result = loose_parse_symbolic(ref, &ref_file);
	} else {
		ref->flags |= GIT_REF_OID;
		result = loose_parse_oid(&ref->target.oid, &ref_file);
	}

	git_buf_free(&ref_file);
	return result;
}

static int loose_lookup_to_packfile(
		struct packref **ref_out,
		git_repository *repo,
		const char *name)
{
	git_buf ref_file = GIT_BUF_INIT;
	struct packref *ref = NULL;
	size_t name_len;

	*ref_out = NULL;

	if (reference_read(&ref_file, NULL, repo->path_repository, name, NULL) < 0)
		return -1;

	git_buf_rtrim(&ref_file);

	name_len = strlen(name);
	ref = git__malloc(sizeof(struct packref) + name_len + 1);
	GITERR_CHECK_ALLOC(ref);

	memcpy(ref->name, name, name_len);
	ref->name[name_len] = 0;

	if (loose_parse_oid(&ref->oid, &ref_file) < 0) {
		git_buf_free(&ref_file);
		git__free(ref);
		return -1;
	}

	ref->flags = GIT_PACKREF_WAS_LOOSE;

	*ref_out = ref;
	git_buf_free(&ref_file);
	return 0;
}

static int loose_write(git_reference *ref)
{
	git_filebuf file = GIT_FILEBUF_INIT;
	git_buf ref_path = GIT_BUF_INIT;
	struct stat st;

	/* Remove a possibly existing empty directory hierarchy
	 * which name would collide with the reference name
	 */
	if (git_futils_rmdir_r(ref->name, ref->owner->path_repository,
		GIT_RMDIR_SKIP_NONEMPTY) < 0)
		return -1;

	if (git_buf_joinpath(&ref_path, ref->owner->path_repository, ref->name) < 0)
		return -1;

	if (git_filebuf_open(&file, ref_path.ptr, GIT_FILEBUF_FORCE) < 0) {
		git_buf_free(&ref_path);
		return -1;
	}

	git_buf_free(&ref_path);

	if (ref->flags & GIT_REF_OID) {
		char oid[GIT_OID_HEXSZ + 1];

		git_oid_fmt(oid, &ref->target.oid);
		oid[GIT_OID_HEXSZ] = '\0';

		git_filebuf_printf(&file, "%s\n", oid);

	} else if (ref->flags & GIT_REF_SYMBOLIC) {
		git_filebuf_printf(&file, GIT_SYMREF "%s\n", ref->target.symbolic);
	} else {
		assert(0); /* don't let this happen */
	}

	if (p_stat(ref_path.ptr, &st) == 0)
		ref->mtime = st.st_mtime;

	return git_filebuf_commit(&file, GIT_REFS_FILE_MODE);
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
		goto corrupt;

	/* Ensure reference is a tag */
	if (git__prefixcmp(tag_ref->name, GIT_REFS_TAGS_DIR) != 0)
		goto corrupt;

	if (buffer + GIT_OID_HEXSZ > buffer_end)
		goto corrupt;

	/* Is this a valid object id? */
	if (git_oid_fromstr(&tag_ref->peel, buffer) < 0)
		goto corrupt;

	buffer = buffer + GIT_OID_HEXSZ;
	if (*buffer == '\r')
		buffer++;

	if (buffer != buffer_end) {
		if (*buffer == '\n')
			buffer++;
		else
			goto corrupt;
	}

	*buffer_out = buffer;
	return 0;

corrupt:
	giterr_set(GITERR_REFERENCE, "The packed references file is corrupted");
	return -1;
}

static int packed_parse_oid(
		struct packref **ref_out,
		const char **buffer_out,
		const char *buffer_end)
{
	struct packref *ref = NULL;

	const char *buffer = *buffer_out;
	const char *refname_begin, *refname_end;

	size_t refname_len;
	git_oid id;

	refname_begin = (buffer + GIT_OID_HEXSZ + 1);
	if (refname_begin >= buffer_end || refname_begin[-1] != ' ')
		goto corrupt;

	/* Is this a valid object id? */
	if (git_oid_fromstr(&id, buffer) < 0)
		goto corrupt;

	refname_end = memchr(refname_begin, '\n', buffer_end - refname_begin);
	if (refname_end == NULL)
		refname_end = buffer_end;

	if (refname_end[-1] == '\r')
		refname_end--;

	refname_len = refname_end - refname_begin;

	ref = git__malloc(sizeof(struct packref) + refname_len + 1);
	GITERR_CHECK_ALLOC(ref);

	memcpy(ref->name, refname_begin, refname_len);
	ref->name[refname_len] = 0;

	git_oid_cpy(&ref->oid, &id);

	ref->flags = 0;

	*ref_out = ref;
	*buffer_out = refname_end + 1;

	return 0;

corrupt:
	git__free(ref);
	giterr_set(GITERR_REFERENCE, "The packed references file is corrupted");
	return -1;
}

static int packed_load(git_repository *repo)
{
	int result, updated;
	git_buf packfile = GIT_BUF_INIT;
	const char *buffer_start, *buffer_end;
	git_refcache *ref_cache = &repo->references;

	/* First we make sure we have allocated the hash table */
	if (ref_cache->packfile == NULL) {
		ref_cache->packfile = git_strmap_alloc();
		GITERR_CHECK_ALLOC(ref_cache->packfile);
	}

	result = reference_read(&packfile, &ref_cache->packfile_time,
		repo->path_repository, GIT_PACKEDREFS_FILE, &updated);

	/*
	 * If we couldn't find the file, we need to clear the table and
	 * return. On any other error, we return that error. If everything
	 * went fine and the file wasn't updated, then there's nothing new
	 * for us here, so just return. Anything else means we need to
	 * refresh the packed refs.
	 */
	if (result == GIT_ENOTFOUND) {
		git_strmap_clear(ref_cache->packfile);
		return 0;
	}

	if (result < 0)
		return -1;

	if (!updated)
		return 0;

	/*
	 * At this point, we want to refresh the packed refs. We already
	 * have the contents in our buffer.
	 */
	git_strmap_clear(ref_cache->packfile);

	buffer_start = (const char *)packfile.ptr;
	buffer_end = (const char *)(buffer_start) + packfile.size;

	while (buffer_start < buffer_end && buffer_start[0] == '#') {
		buffer_start = strchr(buffer_start, '\n');
		if (buffer_start == NULL)
			goto parse_failed;

		buffer_start++;
	}

	while (buffer_start < buffer_end) {
		int err;
		struct packref *ref = NULL;

		if (packed_parse_oid(&ref, &buffer_start, buffer_end) < 0)
			goto parse_failed;

		if (buffer_start[0] == '^') {
			if (packed_parse_peel(ref, &buffer_start, buffer_end) < 0)
				goto parse_failed;
		}

		git_strmap_insert(ref_cache->packfile, ref->name, ref, err);
		if (err < 0)
			goto parse_failed;
	}

	git_buf_free(&packfile);
	return 0;

parse_failed:
	git_strmap_free(ref_cache->packfile);
	ref_cache->packfile = NULL;
	git_buf_free(&packfile);
	return -1;
}


struct dirent_list_data {
	git_repository *repo;
	size_t repo_path_len;
	unsigned int list_flags;

	int (*callback)(const char *, void *);
	void *callback_payload;
	int callback_error;
};

static int _dirent_loose_listall(void *_data, git_buf *full_path)
{
	struct dirent_list_data *data = (struct dirent_list_data *)_data;
	const char *file_path = full_path->ptr + data->repo_path_len;

	if (git_path_isdir(full_path->ptr) == true)
		return git_path_direach(full_path, _dirent_loose_listall, _data);

	/* do not add twice a reference that exists already in the packfile */
	if ((data->list_flags & GIT_REF_PACKED) != 0 &&
		git_strmap_exists(data->repo->references.packfile, file_path))
		return 0;

	if (data->list_flags != GIT_REF_LISTALL) {
		if ((data->list_flags & loose_guess_rtype(full_path)) == 0)
			return 0; /* we are filtering out this reference */
	}

	/* Locked references aren't returned */
	if (!git__suffixcmp(file_path, GIT_FILELOCK_EXTENSION))
		return 0;

	if (data->callback(file_path, data->callback_payload))
		data->callback_error = GIT_EUSER;

	return data->callback_error;
}

static int _dirent_loose_load(void *data, git_buf *full_path)
{
	git_repository *repository = (git_repository *)data;
	void *old_ref = NULL;
	struct packref *ref;
	const char *file_path;
	int err;

	if (git_path_isdir(full_path->ptr) == true)
		return git_path_direach(full_path, _dirent_loose_load, repository);

	file_path = full_path->ptr + strlen(repository->path_repository);

	if (loose_lookup_to_packfile(&ref, repository, file_path) < 0)
		return -1;

	git_strmap_insert2(
		repository->references.packfile, ref->name, ref, old_ref, err);
	if (err < 0) {
		git__free(ref);
		return -1;
	}

	git__free(old_ref);
	return 0;
}

/*
 * Load all the loose references from the repository
 * into the in-memory Packfile, and build a vector with
 * all the references so it can be written back to
 * disk.
 */
static int packed_loadloose(git_repository *repository)
{
	git_buf refs_path = GIT_BUF_INIT;
	int result;

	/* the packfile must have been previously loaded! */
	assert(repository->references.packfile);

	if (git_buf_joinpath(&refs_path, repository->path_repository, GIT_REFS_DIR) < 0)
		return -1;

	/*
	 * Load all the loose files from disk into the Packfile table.
	 * This will overwrite any old packed entries with their
	 * updated loose versions
	 */
	result = git_path_direach(&refs_path, _dirent_loose_load, repository);
	git_buf_free(&refs_path);

	return result;
}

/*
 * Write a single reference into a packfile
 */
static int packed_write_ref(struct packref *ref, git_filebuf *file)
{
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

		if (git_filebuf_printf(file, "%s %s\n^%s\n", oid, ref->name, peel) < 0)
			return -1;
	} else {
		if (git_filebuf_printf(file, "%s %s\n", oid, ref->name) < 0)
			return -1;
	}

	return 0;
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

	if (ref->flags & GIT_PACKREF_HAS_PEEL)
		return 0;

	/*
	 * Only applies to tags, i.e. references
	 * in the /refs/tags folder
	 */
	if (git__prefixcmp(ref->name, GIT_REFS_TAGS_DIR) != 0)
		return 0;

	/*
	 * Find the tagged object in the repository
	 */
	if (git_object_lookup(&object, repo, &ref->oid, GIT_OBJ_ANY) < 0)
		return -1;

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
		git_oid_cpy(&ref->peel, git_tag_target_id(tag));
		ref->flags |= GIT_PACKREF_HAS_PEEL;

		/*
		 * The reference has now cached the resolved OID, and is
		 * marked at such. When written to the packfile, it'll be
		 * accompanied by this resolved oid
		 */
	}

	git_object_free(object);
	return 0;
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
	git_buf full_path = GIT_BUF_INIT;
	int failed = 0;

	for (i = 0; i < packing_list->length; ++i) {
		struct packref *ref = git_vector_get(packing_list, i);

		if ((ref->flags & GIT_PACKREF_WAS_LOOSE) == 0)
			continue;

		if (git_buf_joinpath(&full_path, repo->path_repository, ref->name) < 0)
			return -1; /* critical; do not try to recover on oom */

		if (git_path_exists(full_path.ptr) == true && p_unlink(full_path.ptr) < 0) {
			if (failed)
				continue;

			giterr_set(GITERR_REFERENCE,
				"Failed to remove loose reference '%s' after packing: %s",
				full_path.ptr, strerror(errno));

			failed = 1;
		}

		/*
		 * if we fail to remove a single file, this is *not* good,
		 * but we should keep going and remove as many as possible.
		 * After we've removed as many files as possible, we return
		 * the error code anyway.
		 */
	}

	git_buf_free(&full_path);
	return failed ? -1 : 0;
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
	git_filebuf pack_file = GIT_FILEBUF_INIT;
	unsigned int i;
	git_buf pack_file_path = GIT_BUF_INIT;
	git_vector packing_list;
	unsigned int total_refs;

	assert(repo && repo->references.packfile);

	total_refs =
		(unsigned int)git_strmap_num_entries(repo->references.packfile);

	if (git_vector_init(&packing_list, total_refs, packed_sort) < 0)
		return -1;

	/* Load all the packfile into a vector */
	{
		struct packref *reference;

		/* cannot fail: vector already has the right size */
		git_strmap_foreach_value(repo->references.packfile, reference, {
			git_vector_insert(&packing_list, reference);
		});
	}

	/* sort the vector so the entries appear sorted on the packfile */
	git_vector_sort(&packing_list);

	/* Now we can open the file! */
	if (git_buf_joinpath(&pack_file_path, repo->path_repository, GIT_PACKEDREFS_FILE) < 0)
		goto cleanup_memory;

	if (git_filebuf_open(&pack_file, pack_file_path.ptr, 0) < 0)
		goto cleanup_packfile;

	/* Packfiles have a header... apparently
	 * This is in fact not required, but we might as well print it
	 * just for kicks */
	if (git_filebuf_printf(&pack_file, "%s\n", GIT_PACKEDREFS_HEADER) < 0)
		goto cleanup_packfile;

	for (i = 0; i < packing_list.length; ++i) {
		struct packref *ref = (struct packref *)git_vector_get(&packing_list, i);

		if (packed_find_peel(repo, ref) < 0)
			goto cleanup_packfile;

		if (packed_write_ref(ref, &pack_file) < 0)
			goto cleanup_packfile;
	}

	/* if we've written all the references properly, we can commit
	 * the packfile to make the changes effective */
	if (git_filebuf_commit(&pack_file, GIT_PACKEDREFS_FILE_MODE) < 0)
		goto cleanup_memory;

	/* when and only when the packfile has been properly written,
	 * we can go ahead and remove the loose refs */
	 if (packed_remove_loose(repo, &packing_list) < 0)
		 goto cleanup_memory;

	 {
		struct stat st;
		if (p_stat(pack_file_path.ptr, &st) == 0)
			repo->references.packfile_time = st.st_mtime;
	 }

	git_vector_free(&packing_list);
	git_buf_free(&pack_file_path);

	/* we're good now */
	return 0;

cleanup_packfile:
	git_filebuf_cleanup(&pack_file);

cleanup_memory:
	git_vector_free(&packing_list);
	git_buf_free(&pack_file_path);

	return -1;
}

struct reference_available_t {
	const char *new_ref;
	const char *old_ref;
	int available;
};

static int _reference_available_cb(const char *ref, void *data)
{
	struct reference_available_t *d;

	assert(ref && data);
	d = (struct reference_available_t *)data;

	if (!d->old_ref || strcmp(d->old_ref, ref)) {
		size_t reflen = strlen(ref);
		size_t newlen = strlen(d->new_ref);
		size_t cmplen = reflen < newlen ? reflen : newlen;
		const char *lead = reflen < newlen ? d->new_ref : ref;

		if (!strncmp(d->new_ref, ref, cmplen) && lead[cmplen] == '/') {
			d->available = 0;
			return -1;
		}
	}

	return 0;
}

static int reference_path_available(
	git_repository *repo,
	const char *ref,
	const char* old_ref)
{
	int error;
	struct reference_available_t data;

	data.new_ref = ref;
	data.old_ref = old_ref;
	data.available = 1;

	error = git_reference_foreach(
		repo, GIT_REF_LISTALL, _reference_available_cb, (void *)&data);
	if (error < 0)
		return error;

	if (!data.available) {
		giterr_set(GITERR_REFERENCE,
			"The path to reference '%s' collides with an existing one", ref);
		return -1;
	}

	return 0;
}

static int reference_exists(int *exists, git_repository *repo, const char *ref_name)
{
	git_buf ref_path = GIT_BUF_INIT;

	if (packed_load(repo) < 0)
		return -1;

	if (git_buf_joinpath(&ref_path, repo->path_repository, ref_name) < 0)
		return -1;

	if (git_path_isfile(ref_path.ptr) == true ||
		git_strmap_exists(repo->references.packfile, ref_path.ptr))
	{
		*exists = 1;
	} else {
		*exists = 0;
	}

	git_buf_free(&ref_path);
	return 0;
}

/*
 * Check if a reference could be written to disk, based on:
 *
 *	- Whether a reference with the same name already exists,
 *	and we are allowing or disallowing overwrites
 *
 *	- Whether the name of the reference would collide with
 *	an existing path
 */
static int reference_can_write(
	git_repository *repo,
	const char *refname,
	const char *previous_name,
	int force)
{
	/* see if the reference shares a path with an existing reference;
	 * if a path is shared, we cannot create the reference, even when forcing */
	if (reference_path_available(repo, refname, previous_name) < 0)
		return -1;

	/* check if the reference actually exists, but only if we are not forcing
	 * the rename. If we are forcing, it's OK to overwrite */
	if (!force) {
		int exists;

		if (reference_exists(&exists, repo, refname) < 0)
			return -1;

		/* We cannot proceed if the reference already exists and we're not forcing
		 * the rename; the existing one would be overwritten */
		if (exists) {
			giterr_set(GITERR_REFERENCE,
				"A reference with that name (%s) already exists", refname);
			return GIT_EEXISTS;
		}
	}

	/* FIXME: if the reference exists and we are forcing, do we really need to
	 * remove the reference first?
	 *
	 * Two cases:
	 *
	 *	- the reference already exists and is loose: not a problem, the file
	 *	gets overwritten on disk
	 *
	 *	- the reference already exists and is packed: we write a new one as
	 *	loose, which by all means renders the packed one useless
	 */

	return 0;
}


static int packed_lookup(git_reference *ref)
{
	struct packref *pack_ref = NULL;
	git_strmap *packfile_refs;
	khiter_t pos;

	if (packed_load(ref->owner) < 0)
		return -1;

	/* maybe the packfile hasn't changed at all, so we don't
	 * have to re-lookup the reference */
	if ((ref->flags & GIT_REF_PACKED) &&
		ref->mtime == ref->owner->references.packfile_time)
		return 0;

	if (ref->flags & GIT_REF_SYMBOLIC) {
		git__free(ref->target.symbolic);
		ref->target.symbolic = NULL;
	}

	/* Look up on the packfile */
	packfile_refs = ref->owner->references.packfile;
	pos = git_strmap_lookup_index(packfile_refs, ref->name);
	if (!git_strmap_valid_index(packfile_refs, pos)) {
		giterr_set(GITERR_REFERENCE, "Reference '%s' not found", ref->name);
		return GIT_ENOTFOUND;
	}

	pack_ref = git_strmap_value_at(packfile_refs, pos);

	ref->flags = GIT_REF_OID | GIT_REF_PACKED;
	ref->mtime = ref->owner->references.packfile_time;
	git_oid_cpy(&ref->target.oid, &pack_ref->oid);

	return 0;
}

static int reference_lookup(git_reference *ref)
{
	int result;

	result = loose_lookup(ref);
	if (result == 0)
		return 0;

	/* only try to lookup this reference on the packfile if it
	 * wasn't found on the loose refs; not if there was a critical error */
	if (result == GIT_ENOTFOUND) {
		giterr_clear();
		result = packed_lookup(ref);
		if (result == 0)
			return 0;
	}

	/* unexpected error; free the reference */
	git_reference_free(ref);
	return result;
}

/*
 * Delete a reference.
 * This is an internal method; the reference is removed
 * from disk or the packfile, but the pointer is not freed
 */
static int reference_delete(git_reference *ref)
{
	int result;

	assert(ref);

	/* If the reference is packed, this is an expensive operation.
	 * We need to reload the packfile, remove the reference from the
	 * packing list, and repack */
	if (ref->flags & GIT_REF_PACKED) {
		git_strmap *packfile_refs;
		struct packref *packref;
		khiter_t pos;

		/* load the existing packfile */
		if (packed_load(ref->owner) < 0)
			return -1;

		packfile_refs = ref->owner->references.packfile;
		pos = git_strmap_lookup_index(packfile_refs, ref->name);
		if (!git_strmap_valid_index(packfile_refs, pos)) {
			giterr_set(GITERR_REFERENCE,
				"Reference %s stopped existing in the packfile", ref->name);
			return -1;
		}

		packref = git_strmap_value_at(packfile_refs, pos);
		git_strmap_delete_at(packfile_refs, pos);

		git__free(packref);
		if (packed_write(ref->owner) < 0)
			return -1;

	/* If the reference is loose, we can just remove the reference
	 * from the filesystem */
	} else {
		git_reference *ref_in_pack;
		git_buf full_path = GIT_BUF_INIT;

		if (git_buf_joinpath(&full_path, ref->owner->path_repository, ref->name) < 0)
			return -1;

		result = p_unlink(full_path.ptr);
		git_buf_free(&full_path); /* done with path at this point */

		if (result < 0) {
			giterr_set(GITERR_OS, "Failed to unlink '%s'", full_path.ptr);
			return -1;
		}

		/* When deleting a loose reference, we have to ensure that an older
		 * packed version of it doesn't exist */
		if (git_reference_lookup(&ref_in_pack, ref->owner, ref->name) == 0) {
			assert((ref_in_pack->flags & GIT_REF_PACKED) != 0);
			return git_reference_delete(ref_in_pack);
		}

		giterr_clear();
	}

	return 0;
}

int git_reference_delete(git_reference *ref)
{
	int result = reference_delete(ref);
	git_reference_free(ref);
	return result;
}

int git_reference_lookup(git_reference **ref_out,
	git_repository *repo, const char *name)
{
	return git_reference_lookup_resolved(ref_out, repo, name, 0);
}

int git_reference_name_to_id(
	git_oid *out, git_repository *repo, const char *name)
{
	int error;
	git_reference *ref;

	if ((error = git_reference_lookup_resolved(&ref, repo, name, -1)) < 0)
		return error;

	git_oid_cpy(out, git_reference_target(ref));
	git_reference_free(ref);
	return 0;
}

int git_reference_lookup_resolved(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	int max_nesting)
{
	git_reference *scan;
	int result, nesting;

	assert(ref_out && repo && name);

	*ref_out = NULL;

	if (max_nesting > MAX_NESTING_LEVEL)
		max_nesting = MAX_NESTING_LEVEL;
	else if (max_nesting < 0)
		max_nesting = DEFAULT_NESTING_LEVEL;

	scan = git__calloc(1, sizeof(git_reference));
	GITERR_CHECK_ALLOC(scan);

	scan->name = git__calloc(GIT_REFNAME_MAX + 1, sizeof(char));
	GITERR_CHECK_ALLOC(scan->name);

	if ((result = git_reference__normalize_name_lax(
		scan->name,
		GIT_REFNAME_MAX,
		name)) < 0) {
			git_reference_free(scan);
			return result;
	}

	scan->target.symbolic = git__strdup(scan->name);
	GITERR_CHECK_ALLOC(scan->target.symbolic);

	scan->owner = repo;
	scan->flags = GIT_REF_SYMBOLIC;

	for (nesting = max_nesting;
		 nesting >= 0 && (scan->flags & GIT_REF_SYMBOLIC) != 0;
		 nesting--)
	{
		if (nesting != max_nesting)
			strncpy(scan->name, scan->target.symbolic, GIT_REFNAME_MAX);

		scan->mtime = 0;

		if ((result = reference_lookup(scan)) < 0)
			return result; /* lookup git_reference_free on scan already */
	}

	if ((scan->flags & GIT_REF_OID) == 0 && max_nesting != 0) {
		giterr_set(GITERR_REFERENCE,
			"Cannot resolve reference (>%u levels deep)", max_nesting);
		git_reference_free(scan);
		return -1;
	}

	*ref_out = scan;
	return 0;
}

/**
 * Getters
 */
git_ref_t git_reference_type(const git_reference *ref)
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

const char *git_reference_name(const git_reference *ref)
{
	assert(ref);
	return ref->name;
}

git_repository *git_reference_owner(const git_reference *ref)
{
	assert(ref);
	return ref->owner;
}

const git_oid *git_reference_target(const git_reference *ref)
{
	assert(ref);

	if ((ref->flags & GIT_REF_OID) == 0)
		return NULL;

	return &ref->target.oid;
}

const char *git_reference_symbolic_target(const git_reference *ref)
{
	assert(ref);

	if ((ref->flags & GIT_REF_SYMBOLIC) == 0)
		return NULL;

	return ref->target.symbolic;
}

int git_reference_symbolic_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const char *target,
	int force)
{
	char normalized[GIT_REFNAME_MAX];
	git_reference *ref = NULL;
	int error;

	if ((error = git_reference__normalize_name_lax(
		normalized,
		sizeof(normalized),
		name)) < 0)
			return error;

	if ((error = reference_can_write(repo, normalized, NULL, force)) < 0)
		return error;

	if (reference_alloc(&ref, repo, normalized) < 0)
		return -1;

	ref->flags |= GIT_REF_SYMBOLIC;

	/* set the target; this will normalize the name automatically
	 * and write the reference on disk */
	if (git_reference_symbolic_set_target(ref, target) < 0) {
		git_reference_free(ref);
		return -1;
	}
	if (ref_out == NULL) {
		git_reference_free(ref);
	} else {
		*ref_out = ref;
	}

	return 0;
}

int git_reference_create(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const git_oid *id,
	int force)
{
	int error;
	git_reference *ref = NULL;
	char normalized[GIT_REFNAME_MAX];

	if ((error = git_reference__normalize_name_lax(
		normalized,
		sizeof(normalized),
		name)) < 0)
			return error;

	if ((error = reference_can_write(repo, normalized, NULL, force)) < 0)
		return error;

	if (reference_alloc(&ref, repo, name) < 0)
		return -1;

	ref->flags |= GIT_REF_OID;

	/* set the oid; this will write the reference on disk */
	if (git_reference_set_target(ref, id) < 0) {
		git_reference_free(ref);
		return -1;
	}

	if (ref_out == NULL) {
		git_reference_free(ref);
	} else {
		*ref_out = ref;
	}

	return 0;
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
int git_reference_set_target(git_reference *ref, const git_oid *id)
{
	git_odb *odb = NULL;

	if ((ref->flags & GIT_REF_OID) == 0) {
		giterr_set(GITERR_REFERENCE, "Cannot set OID on symbolic reference");
		return -1;
	}

	assert(ref->owner);

	if (git_repository_odb__weakptr(&odb, ref->owner) < 0)
		return -1;

	/* Don't let the user create references to OIDs that
	 * don't exist in the ODB */
	if (!git_odb_exists(odb, id)) {
		giterr_set(GITERR_REFERENCE,
			"Target OID for the reference doesn't exist on the repository");
		return -1;
	}

	/* Update the OID value on `ref` */
	git_oid_cpy(&ref->target.oid, id);

	/* Write back to disk */
	return loose_write(ref);
}

/*
 * Change the target of a symbolic reference.
 *
 * This is easy because symrefs cannot be inside
 * a pack. We just change the target in memory
 * and overwrite the file on disk.
 */
int git_reference_symbolic_set_target(git_reference *ref, const char *target)
{
	int error;
	char normalized[GIT_REFNAME_MAX];

	if ((ref->flags & GIT_REF_SYMBOLIC) == 0) {
		giterr_set(GITERR_REFERENCE,
			"Cannot set symbolic target on a direct reference");
		return -1;
	}

	if ((error = git_reference__normalize_name_lax(
		normalized,
		sizeof(normalized),
		target)) < 0)
			return error;

	git__free(ref->target.symbolic);
	ref->target.symbolic = git__strdup(normalized);
	GITERR_CHECK_ALLOC(ref->target.symbolic);

	return loose_write(ref);
}

int git_reference_rename(git_reference *ref, const char *new_name, int force)
{
	int result;
	unsigned int normalization_flags;
	git_buf aux_path = GIT_BUF_INIT;
	char normalized[GIT_REFNAME_MAX];
	bool should_head_be_updated = false;

	normalization_flags = ref->flags & GIT_REF_SYMBOLIC ?
		GIT_REF_FORMAT_ALLOW_ONELEVEL
		: GIT_REF_FORMAT_NORMAL;

	if ((result = git_reference_normalize_name(
		normalized,
		sizeof(normalized),
		new_name,
		normalization_flags)) < 0)
			return result;

	if ((result = reference_can_write(ref->owner, normalized, ref->name, force)) < 0)
		return result;

	/* Initialize path now so we won't get an allocation failure once
	 * we actually start removing things. */
	if (git_buf_joinpath(&aux_path, ref->owner->path_repository, new_name) < 0)
		return -1;

	/*
	 * Check if we have to update HEAD.
	 */
	if ((should_head_be_updated = git_branch_is_head(ref)) < 0)
		goto cleanup;

	/*
	 * Now delete the old ref and remove an possibly existing directory
	 * named `new_name`. Note that using the internal `reference_delete`
	 * method deletes the ref from disk but doesn't free the pointer, so
	 * we can still access the ref's attributes for creating the new one
	 */
	if (reference_delete(ref) < 0)
		goto cleanup;

	/*
	 * Finally we can create the new reference.
	 */
	if (ref->flags & GIT_REF_SYMBOLIC) {
		result = git_reference_symbolic_create(
			NULL, ref->owner, new_name, ref->target.symbolic, force);
	} else {
		result = git_reference_create(
			NULL, ref->owner, new_name, &ref->target.oid, force);
	}

	if (result < 0)
		goto rollback;

	/*
	 * Update HEAD it was poiting to the reference being renamed.
	 */
	if (should_head_be_updated && 
		git_repository_set_head(ref->owner, new_name) < 0) {
			giterr_set(GITERR_REFERENCE,
				"Failed to update HEAD after renaming reference");
			goto cleanup;
	}

	/*
	 * Rename the reflog file, if it exists.
	 */
	if ((git_reference_has_log(ref)) && (git_reflog_rename(ref, new_name) < 0))
		goto cleanup;

	/*
	 * Change the name of the reference given by the user.
	 */
	git__free(ref->name);
	ref->name = git__strdup(new_name);

	/* The reference is no longer packed */
	ref->flags &= ~GIT_REF_PACKED;

	git_buf_free(&aux_path);
	return 0;

cleanup:
	git_buf_free(&aux_path);
	return -1;

rollback:
	/*
	 * Try to create the old reference again, ignore failures
	 */
	if (ref->flags & GIT_REF_SYMBOLIC)
		git_reference_symbolic_create(
			NULL, ref->owner, ref->name, ref->target.symbolic, 0);
	else
		git_reference_create(
			NULL, ref->owner, ref->name, &ref->target.oid, 0);

	/* The reference is no longer packed */
	ref->flags &= ~GIT_REF_PACKED;

	git_buf_free(&aux_path);
	return -1;
}

int git_reference_resolve(git_reference **ref_out, const git_reference *ref)
{
	if (ref->flags & GIT_REF_OID)
		return git_reference_lookup(ref_out, ref->owner, ref->name);
	else
		return git_reference_lookup_resolved(ref_out, ref->owner, ref->target.symbolic, -1);
}

int git_reference_packall(git_repository *repo)
{
	if (packed_load(repo) < 0 || /* load the existing packfile */
		packed_loadloose(repo) < 0 || /* add all the loose refs */
		packed_write(repo) < 0) /* write back to disk */
		return -1;

	return 0;
}

int git_reference_foreach(
	git_repository *repo,
	unsigned int list_flags,
	git_reference_foreach_cb callback,
	void *payload)
{
	int result;
	struct dirent_list_data data;
	git_buf refs_path = GIT_BUF_INIT;

	/* list all the packed references first */
	if (list_flags & GIT_REF_PACKED) {
		const char *ref_name;
		void *ref = NULL;
		GIT_UNUSED(ref);

		if (packed_load(repo) < 0)
			return -1;

		git_strmap_foreach(repo->references.packfile, ref_name, ref, {
			if (callback(ref_name, payload))
				return GIT_EUSER;
		});
	}

	/* now list the loose references, trying not to
	 * duplicate the ref names already in the packed-refs file */

	data.repo_path_len = strlen(repo->path_repository);
	data.list_flags = list_flags;
	data.repo = repo;
	data.callback = callback;
	data.callback_payload = payload;
	data.callback_error = 0;

	if (git_buf_joinpath(&refs_path, repo->path_repository, GIT_REFS_DIR) < 0)
		return -1;

	result = git_path_direach(&refs_path, _dirent_loose_listall, &data);

	git_buf_free(&refs_path);

	return data.callback_error ? GIT_EUSER : result;
}

static int cb__reflist_add(const char *ref, void *data)
{
	return git_vector_insert((git_vector *)data, git__strdup(ref));
}

int git_reference_list(
	git_strarray *array,
	git_repository *repo,
	unsigned int list_flags)
{
	git_vector ref_list;

	assert(array && repo);

	array->strings = NULL;
	array->count = 0;

	if (git_vector_init(&ref_list, 8, NULL) < 0)
		return -1;

	if (git_reference_foreach(
			repo, list_flags, &cb__reflist_add, (void *)&ref_list) < 0) {
		git_vector_free(&ref_list);
		return -1;
	}

	array->strings = (char **)ref_list.contents;
	array->count = ref_list.length;
	return 0;
}

int git_reference_reload(git_reference *ref)
{
	return reference_lookup(ref);
}

void git_repository__refcache_free(git_refcache *refs)
{
	assert(refs);

	if (refs->packfile) {
		struct packref *reference;

		git_strmap_foreach_value(refs->packfile, reference, {
			git__free(reference);
		});

		git_strmap_free(refs->packfile);
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

static int ensure_segment_validity(const char *name)
{
	const char *current = name;
	char prev = '\0';
	int lock_len = strlen(GIT_FILELOCK_EXTENSION);

	if (*current == '.')
		return -1; /* Refname starts with "." */

	for (current = name; ; current++) {
		if (*current == '\0' || *current == '/')
			break;

		if (!is_valid_ref_char(*current))
			return -1; /* Illegal character in refname */

		if (prev == '.' && *current == '.')
			return -1; /* Refname contains ".." */

		if (prev == '@' && *current == '{')
			return -1; /* Refname contains "@{" */

		prev = *current;
	}

	/* A refname component can not end with ".lock" */
	if (current - name >= lock_len &&
		!memcmp(current - lock_len, GIT_FILELOCK_EXTENSION, lock_len))
			return -1;

	return (int)(current - name);
}

static bool is_all_caps_and_underscore(const char *name, size_t len)
{
	size_t i;
	char c;

	assert(name && len > 0);

	for (i = 0; i < len; i++)
	{
		c = name[i];
		if ((c < 'A' || c > 'Z') && c != '_')
			return false;
	}

	if (*name == '_' || name[len - 1] == '_')
		return false;

	return true;
}

int git_reference__normalize_name(
	git_buf *buf,
	const char *name,
	unsigned int flags)
{
	// Inspired from https://github.com/git/git/blob/f06d47e7e0d9db709ee204ed13a8a7486149f494/refs.c#L36-100

	char *current;
	int segment_len, segments_count = 0, error = GIT_EINVALIDSPEC;
	unsigned int process_flags;
	bool normalize = (buf != NULL);
	assert(name);

	process_flags = flags;
	current = (char *)name;

	if (normalize)
		git_buf_clear(buf);

	while (true) {
		segment_len = ensure_segment_validity(current);
		if (segment_len < 0) {
			if ((process_flags & GIT_REF_FORMAT_REFSPEC_PATTERN) &&
					current[0] == '*' &&
					(current[1] == '\0' || current[1] == '/')) {
				/* Accept one wildcard as a full refname component. */
				process_flags &= ~GIT_REF_FORMAT_REFSPEC_PATTERN;
				segment_len = 1;
			} else
				goto cleanup;
		}

		if (segment_len > 0) {
			if (normalize) {
				size_t cur_len = git_buf_len(buf);

				git_buf_joinpath(buf, git_buf_cstr(buf), current);
				git_buf_truncate(buf,
					cur_len + segment_len + (segments_count ? 1 : 0));

				if (git_buf_oom(buf)) {
					error = -1;
					goto cleanup;
				}
			}

			segments_count++;
		}

		/* No empty segment is allowed when not normalizing */
		if (segment_len == 0 && !normalize)
			goto cleanup;
		
		if (current[segment_len] == '\0')
			break;

		current += segment_len + 1;
	}

	/* A refname can not be empty */
	if (segment_len == 0 && segments_count == 0)
		goto cleanup;

	/* A refname can not end with "." */
	if (current[segment_len - 1] == '.')
		goto cleanup;

	/* A refname can not end with "/" */
	if (current[segment_len - 1] == '/')
		goto cleanup;

	if ((segments_count == 1 ) && !(flags & GIT_REF_FORMAT_ALLOW_ONELEVEL))
		goto cleanup;

	if ((segments_count == 1 ) &&
		!(is_all_caps_and_underscore(name, (size_t)segment_len) ||
			((flags & GIT_REF_FORMAT_REFSPEC_PATTERN) && !strcmp("*", name))))
			goto cleanup;

	if ((segments_count > 1)
		&& (is_all_caps_and_underscore(name, strchr(name, '/') - name)))
			goto cleanup;

	error = 0;

cleanup:
	if (error == GIT_EINVALIDSPEC)
		giterr_set(
			GITERR_REFERENCE,
			"The given reference name '%s' is not valid", name);

	if (error && normalize)
		git_buf_free(buf);

	return error;
}

int git_reference_normalize_name(
	char *buffer_out,
	size_t buffer_size,
	const char *name,
	unsigned int flags)
{
	git_buf buf = GIT_BUF_INIT;
	int error;

	if ((error = git_reference__normalize_name(&buf, name, flags)) < 0)
		goto cleanup;

	if (git_buf_len(&buf) > buffer_size - 1) {
		giterr_set(
		GITERR_REFERENCE,
		"The provided buffer is too short to hold the normalization of '%s'", name);
		error = GIT_EBUFS;
		goto cleanup;
	}

	git_buf_copy_cstr(buffer_out, buffer_size, &buf);

	error = 0;

cleanup:
	git_buf_free(&buf);
	return error;
}

int git_reference__normalize_name_lax(
	char *buffer_out,
	size_t out_size,
	const char *name)
{
	return git_reference_normalize_name(
		buffer_out,
		out_size,
		name,
		GIT_REF_FORMAT_ALLOW_ONELEVEL);
}
#define GIT_REF_TYPEMASK (GIT_REF_OID | GIT_REF_SYMBOLIC)

int git_reference_cmp(git_reference *ref1, git_reference *ref2)
{
	assert(ref1 && ref2);

	/* let's put symbolic refs before OIDs */
	if ((ref1->flags & GIT_REF_TYPEMASK) != (ref2->flags & GIT_REF_TYPEMASK))
		return (ref1->flags & GIT_REF_SYMBOLIC) ? -1 : 1;

	if (ref1->flags & GIT_REF_SYMBOLIC)
		return strcmp(ref1->target.symbolic, ref2->target.symbolic);

	return git_oid_cmp(&ref1->target.oid, &ref2->target.oid);
}

/* Update the reference named `ref_name` so it points to `oid` */
int git_reference__update(git_repository *repo, const git_oid *oid, const char *ref_name)
{
	git_reference *ref;
	int res;

	res = git_reference_lookup(&ref, repo, ref_name);

	/* If we haven't found the reference at all, we assume we need to create
	 * a new reference and that's it */
	if (res == GIT_ENOTFOUND) {
		giterr_clear();
		return git_reference_create(NULL, repo, ref_name, oid, 1);
	}

	if (res < 0)
		return -1;

	/* If we have found a reference, but it's symbolic, we need to update
	 * the direct reference it points to */
	if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
		git_reference *aux;
		const char *sym_target;

		/* The target pointed at by this reference */
		sym_target = git_reference_symbolic_target(ref);

		/* resolve the reference to the target it points to */
		res = git_reference_resolve(&aux, ref);

		/*
		 * if the symbolic reference pointed to an inexisting ref,
		 * this is means we're creating a new branch, for example.
		 * We need to create a new direct reference with that name
		 */
		if (res == GIT_ENOTFOUND) {
			giterr_clear();
			res = git_reference_create(NULL, repo, sym_target, oid, 1);
			git_reference_free(ref);
			return res;
		}

		/* free the original symbolic reference now; not before because
		 * we're using the `sym_target` pointer */
		git_reference_free(ref);

		if (res < 0)
			return -1;

		/* store the newly found direct reference in its place */
		ref = aux;
	}

	/* ref is made to point to `oid`: ref is either the original reference,
	 * or the target of the symbolic reference we've looked up */
	res = git_reference_set_target(ref, oid);
	git_reference_free(ref);
	return res;
}

struct glob_cb_data {
	const char *glob;
	int (*callback)(const char *, void *);
	void *payload;
};

static int fromglob_cb(const char *reference_name, void *payload)
{
	struct glob_cb_data *data = (struct glob_cb_data *)payload;

	if (!p_fnmatch(data->glob, reference_name, 0))
		return data->callback(reference_name, data->payload);

	return 0;
}

int git_reference_foreach_glob(
	git_repository *repo,
	const char *glob,
	unsigned int list_flags,
	int (*callback)(
		const char *reference_name,
		void *payload),
	void *payload)
{
	struct glob_cb_data data;

	assert(repo && glob && callback);

	data.glob = glob;
	data.callback = callback;
	data.payload = payload;

	return git_reference_foreach(
			repo, list_flags, fromglob_cb, &data);
}

int git_reference_has_log(
	git_reference *ref)
{
	git_buf path = GIT_BUF_INIT;
	int result;

	assert(ref);

	if (git_buf_join_n(&path, '/', 3, ref->owner->path_repository, GIT_REFLOG_DIR, ref->name) < 0)
		return -1;

	result = git_path_isfile(git_buf_cstr(&path));
	git_buf_free(&path);

	return result;
}

int git_reference__is_branch(const char *ref_name)
{
	return git__prefixcmp(ref_name, GIT_REFS_HEADS_DIR) == 0;
}

int git_reference_is_branch(git_reference *ref)
{
	assert(ref);
	return git_reference__is_branch(ref->name);
}

int git_reference__is_remote(const char *ref_name)
{
	return git__prefixcmp(ref_name, GIT_REFS_REMOTES_DIR) == 0;
}

int git_reference_is_remote(git_reference *ref)
{
	assert(ref);
	return git_reference__is_remote(ref->name);
}

static int peel_error(int error, git_reference *ref, const char* msg)
{
	giterr_set(
		GITERR_INVALID,
		"The reference '%s' cannot be peeled - %s", git_reference_name(ref), msg);
	return error;
}

static int reference_target(git_object **object, git_reference *ref)
{
	const git_oid *oid;

	oid = git_reference_target(ref);

	return git_object_lookup(object, git_reference_owner(ref), oid, GIT_OBJ_ANY);
}

int git_reference_peel(
		git_object **peeled,
		git_reference *ref,
		git_otype target_type)
{
	git_reference *resolved = NULL;
	git_object *target = NULL;
	int error;

	assert(ref);

	if ((error = git_reference_resolve(&resolved, ref)) < 0)
		return peel_error(error, ref, "Cannot resolve reference");

	if ((error = reference_target(&target, resolved)) < 0) {
		peel_error(error, ref, "Cannot retrieve reference target");
		goto cleanup;
	}

	if (target_type == GIT_OBJ_ANY && git_object_type(target) != GIT_OBJ_TAG)
		error = git_object__dup(peeled, target);
	else
		error = git_object_peel(peeled, target, target_type);

cleanup:
	git_object_free(target);
	git_reference_free(resolved);
	return error;
}

int git_reference__is_valid_name(
	const char *refname,
	unsigned int flags)
{
	int error;

	error = git_reference__normalize_name(NULL, refname, flags) == 0;
	giterr_clear();

	return error;
}

int git_reference_is_valid_name(
	const char *refname)
{
	return git_reference__is_valid_name(
		refname,
		GIT_REF_FORMAT_ALLOW_ONELEVEL);
}
