/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_object_h__
#define INCLUDE_object_h__

/** Base git object for inheritance */
struct git_object {
	git_cached_obj cached;
	git_repository *repo;
};

/* fully free the object; internal method, DO NOT EXPORT */
void git_object__free(void *object);

int git_object__from_odb_object(
	git_object **object_out,
	git_repository *repo,
	git_odb_object *odb_obj,
	git_otype type);

int git_object__resolve_to_type(git_object **obj, git_otype type);

int git_oid__parse(git_oid *oid, const char **buffer_out, const char *buffer_end, const char *header);

void git_oid__writebuf(git_buf *buf, const char *header, const git_oid *oid);

#define GIT_OBJ_WRAPPER(TYPE,OBJTYPE) \
	int TYPE##_lookup(TYPE **out, git_repository *repo, const git_oid *id) { \
		return git_object_lookup((git_object **)out, repo, id, OBJTYPE); } \
	int TYPE##_lookup_prefix(TYPE **out, git_repository *repo, const git_oid *id, size_t len) { \
		return git_object_lookup_prefix((git_object **)out, repo, id, len, OBJTYPE); } \
	void TYPE##_free(TYPE *obj) { \
		git_object_free((git_object *)obj); } \
	const git_oid *TYPE##_id(const TYPE *obj) { \
		return git_object_id((const git_object *)obj); } \
	git_repository *TYPE##_owner(const TYPE *obj) { \
		return git_object_owner((const git_object *)obj); }

#endif
