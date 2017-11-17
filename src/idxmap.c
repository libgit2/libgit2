/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "idxmap.h"

/* This is __ac_X31_hash_string but with tolower and it takes the entry's stage into account */
static kh_inline khint_t idxentry_hash(const void *ptr)
{
	const git_index_entry *e = *(const git_index_entry **) ptr;
	const char *s = e->path;
	khint_t h = (khint_t)git__tolower(*s);
	if (h) for (++s ; *s; ++s) h = (h << 5) - h + (khint_t)git__tolower(*s);
	return h + GIT_IDXENTRY_STAGE(e);
}

static kh_inline int idxentry_equal(const void *aptr, const void *bptr)
{
    const git_index_entry *a = *(const git_index_entry **) aptr, *b = *(const git_index_entry **) bptr;
    return (GIT_IDXENTRY_STAGE(a) == GIT_IDXENTRY_STAGE(b) && strcmp(a->path, b->path) == 0);
}

static kh_inline int idxentry_icase_equal(const void *aptr, const void *bptr)
{
    const git_index_entry *a = *(const git_index_entry **) aptr, *b = *(const git_index_entry **) bptr;
    return (GIT_IDXENTRY_STAGE(a) == GIT_IDXENTRY_STAGE(b) && strcasecmp(a->path, b->path) == 0);
}

int git_idxmap_alloc(git_idxmap **map, bool ignore_case)
{
	if ((*map = kh_init(sizeof(const git_index_entry *), sizeof(git_index_entry *), idxentry_hash, idxentry_equal, true)) == NULL) {
		giterr_set_oom();
		return -1;
	}

	git_idxmap_set_ignore_case(*map, ignore_case);

	return 0;
}

void git_idxmap_set_ignore_case(git_idxmap *map, bool ignore_case)
{
	map->hash_equal = ignore_case ?
		idxentry_icase_equal : idxentry_equal;
}

void git_idxmap_insert(git_idxmap *map, const git_index_entry *key, void *value, int *rval)
{
	khiter_t idx = kh_put(map, &key, rval);

	if ((*rval) >= 0) {
		if ((*rval) == 0)
			memcpy(&kh_key(map, idx), &key, map->keysize);
		memcpy(&kh_val(map, idx), &value, map->valsize);
	}
}

size_t git_idxmap_lookup_index(git_idxmap *map, const git_index_entry *key)
{
	return kh_get(map, &key);
}

void *git_idxmap_value_at(git_idxmap *map, size_t idx)
{
	return kh_val(map, idx);
}

int git_idxmap_valid_index(git_idxmap *map, size_t idx)
{
	return idx != kh_end(map);
}

int git_idxmap_has_data(git_idxmap *map, size_t idx)
{
	return kh_exist(map, idx);
}

void git_idxmap_resize(git_idxmap *map, size_t size)
{
	kh_resize(map, size);
}

void git_idxmap_free(git_idxmap *map)
{
	kh_destroy(map);
}

void git_idxmap_clear(git_idxmap *map)
{
	kh_clear(map);
}

void git_idxmap_delete_at(git_idxmap *map, size_t idx)
{
	kh_del(map, idx);
}

void git_idxmap_delete(git_idxmap *map, const git_index_entry *key)
{
	khiter_t idx = git_idxmap_lookup_index(map, key);
	if (git_idxmap_valid_index(map, idx))
		git_idxmap_delete_at(map, idx);
}
