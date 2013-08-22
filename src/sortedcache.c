#include "sortedcache.h"

GIT__USE_STRMAP;

int git_sortedcache_new(
	git_sortedcache **out,
	size_t item_path_offset,
	git_sortedcache_free_item_fn free_item,
	void *free_item_payload,
	git_vector_cmp item_cmp,
	const char *path)
{
	git_sortedcache *sc;
	size_t pathlen;

	pathlen = path ? strlen(path) : 0;

	sc = git__calloc(sizeof(git_sortedcache) + pathlen + 1, 1);
	GITERR_CHECK_ALLOC(sc);

	if (git_pool_init(&sc->pool, 1, 0) < 0 ||
		git_vector_init(&sc->items, 4, item_cmp) < 0 ||
		(sc->map = git_strmap_alloc()) == NULL)
		goto fail;

	if (git_mutex_init(&sc->lock)) {
		giterr_set(GITERR_OS, "Failed to initialize mutex");
		goto fail;
	}

	sc->item_path_offset = item_path_offset;
	sc->free_item = free_item;
	sc->free_item_payload = free_item_payload;
	GIT_REFCOUNT_INC(sc);
	if (pathlen)
		memcpy(sc->path, path, pathlen);

	*out = sc;
	return 0;

fail:
	if (sc->map)
		git_strmap_free(sc->map);
	git_vector_free(&sc->items);
	git_pool_clear(&sc->pool);
	git__free(sc);
	return -1;
}

void git_sortedcache_incref(git_sortedcache *sc)
{
	GIT_REFCOUNT_INC(sc);
}

static void sortedcache_clear(git_sortedcache *sc)
{
	git_strmap_clear(sc->map);

	if (sc->free_item) {
		size_t i;
		void *item;

		git_vector_foreach(&sc->items, i, item) {
			sc->free_item(sc->free_item_payload, item);
		}
	}

	git_vector_clear(&sc->items);

	git_pool_clear(&sc->pool);
}

static void sortedcache_free(git_sortedcache *sc)
{
	if (git_mutex_lock(&sc->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to acquire mutex lock for free");
		return;
	}

	sortedcache_clear(sc);

	git_vector_free(&sc->items);
	git_strmap_free(sc->map);

	git_mutex_unlock(&sc->lock);
	git_mutex_free(&sc->lock);

	git__free(sc);
}

void git_sortedcache_free(git_sortedcache *sc)
{
	if (!sc)
		return;
	GIT_REFCOUNT_DEC(sc, sortedcache_free);
}

static int sortedcache_copy_item(void *payload, void *tgt_item, void *src_item)
{
	git_sortedcache *sc = payload;
	/* path will already have been copied by upsert */
	memcpy(tgt_item, src_item, sc->item_path_offset);
	return 0;
}

/* copy a sorted cache */
int git_sortedcache_copy(
	git_sortedcache **out,
	git_sortedcache *src,
	int (*copy_item)(void *payload, void *tgt_item, void *src_item),
	void *payload)
{
	git_sortedcache *tgt;
	size_t i;
	void *src_item, *tgt_item;

	if (!copy_item) {
		copy_item = sortedcache_copy_item;
		payload   = src;
	}

	if (git_sortedcache_new(
			&tgt, src->item_path_offset,
			src->free_item, src->free_item_payload,
			src->items._cmp, src->path) < 0)
		return -1;

	if (git_sortedcache_lock(src) < 0) {
		git_sortedcache_free(tgt);
		return -1;
	}

	git_vector_foreach(&src->items, i, src_item) {
		if (git_sortedcache_upsert(
				&tgt_item, tgt, ((char *)src_item) + src->item_path_offset) < 0)
			goto fail;
		if (copy_item(payload, tgt_item, src_item) < 0)
			goto fail;
	}

	git_sortedcache_unlock(src);

	*out = tgt;
	return 0;

fail:
	git_sortedcache_unlock(src);
	git_sortedcache_free(tgt);
	return -1;
}

/* release all items in sorted cache */
void git_sortedcache_clear(git_sortedcache *sc, bool lock)
{
	if (lock && git_mutex_lock(&sc->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to acquire mutex lock for clear");
		return;
	}

	sortedcache_clear(sc);

	if (lock)
		git_mutex_unlock(&sc->lock);
}

/* check file stamp to see if reload is required */
bool git_sortedcache_out_of_date(git_sortedcache *sc)
{
	return (git_futils_filestamp_check(&sc->stamp, sc->path) != 0);
}

/* lock sortedcache while making modifications */
int git_sortedcache_lock(git_sortedcache *sc)
{
	GIT_UNUSED(sc); /* to prevent warning when compiled w/o threads */

	if (git_mutex_lock(&sc->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to acquire mutex lock");
		return -1;
	}
	return 0;
}

/* unlock sorted cache when done with modifications */
int git_sortedcache_unlock(git_sortedcache *sc)
{
	git_vector_sort(&sc->items);
	git_mutex_unlock(&sc->lock);
	return 0;
}

/* if the file has changed, lock cache and load file contents into buf;
 * returns <0 on error, >0 if file has not changed
 */
int git_sortedcache_lockandload(git_sortedcache *sc, git_buf *buf)
{
	int error, fd;

	if ((error = git_sortedcache_lock(sc)) < 0)
		return error;

	if ((error = git_futils_filestamp_check(&sc->stamp, sc->path)) <= 0)
		goto unlock;

	if (!git__is_sizet(sc->stamp.size)) {
		giterr_set(GITERR_INVALID, "Unable to load file larger than size_t");
		error = -1;
		goto unlock;
	}

	if ((fd = git_futils_open_ro(sc->path)) < 0) {
		error = fd;
		goto unlock;
	}

	if (buf)
		error = git_futils_readbuffer_fd(buf, fd, (size_t)sc->stamp.size);

	(void)p_close(fd);

	if (error < 0)
		goto unlock;

	return 1; /* return 1 -> file needs reload and was successfully loaded */

unlock:
	git_sortedcache_unlock(sc);
	return error;
}

/* find and/or insert item, returning pointer to item data */
int git_sortedcache_upsert(
	void **out, git_sortedcache *sc, const char *key)
{
	int error = 0;
	khiter_t pos;
	void *item;
	size_t keylen;
	char *item_key;

	pos = git_strmap_lookup_index(sc->map, key);
	if (git_strmap_valid_index(sc->map, pos)) {
		item = git_strmap_value_at(sc->map, pos);
		goto done;
	}

	keylen = strlen(key);
	item = git_pool_mallocz(&sc->pool, sc->item_path_offset + keylen + 1);
	GITERR_CHECK_ALLOC(item);

	/* one strange thing is that even if the vector or hash table insert
	 * fail, there is no way to free the pool item so we just abandon it
	 */

	item_key = ((char *)item) + sc->item_path_offset;
	memcpy(item_key, key, keylen);

	pos = kh_put(str, sc->map, item_key, &error);
	if (error < 0)
		goto done;

	if (!error)
		kh_key(sc->map, pos) = item_key;
	kh_val(sc->map, pos) = item;

	error = git_vector_insert(&sc->items, item);
	if (error < 0)
		git_strmap_delete_at(sc->map, pos);

done:
	if (out)
		*out = !error ? item : NULL;
	return error;
}

/* lookup item by key */
void *git_sortedcache_lookup(const git_sortedcache *sc, const char *key)
{
	khiter_t pos = git_strmap_lookup_index(sc->map, key);
	if (git_strmap_valid_index(sc->map, pos))
		return git_strmap_value_at(sc->map, pos);
	return NULL;
}

/* find out how many items are in the cache */
size_t git_sortedcache_entrycount(const git_sortedcache *sc)
{
	return git_vector_length(&sc->items);
}

/* lookup item by index */
void *git_sortedcache_entry(const git_sortedcache *sc, size_t pos)
{
	return git_vector_get(&sc->items, pos);
}

struct sortedcache_magic_key {
	size_t offset;
	const char *key;
};

static int sortedcache_magic_cmp(const void *key, const void *value)
{
	const struct sortedcache_magic_key *magic = key;
	const char *value_key = ((const char *)value) + magic->offset;
	return strcmp(magic->key, value_key);
}

/* lookup index of item by key */
int git_sortedcache_lookup_index(
	size_t *out, git_sortedcache *sc, const char *key)
{
	struct sortedcache_magic_key magic;

	magic.offset = sc->item_path_offset;
	magic.key    = key;

	return git_vector_bsearch2(out, &sc->items, sortedcache_magic_cmp, &magic);
}

/* remove entry from cache */
int git_sortedcache_remove(git_sortedcache *sc, size_t pos, bool lock)
{
	int error = 0;
	char *item;
	khiter_t mappos;

	if (lock && git_sortedcache_lock(sc) < 0)
		return -1;

	/* because of pool allocation, this can't actually remove the item,
	 * but we can remove it from the items vector and the hash table.
	 */

	if ((item = git_vector_get(&sc->items, pos)) == NULL) {
		giterr_set(GITERR_INVALID, "Removing item out of range");
		error = GIT_ENOTFOUND;
		goto done;
	}

	(void)git_vector_remove(&sc->items, pos);

	mappos = git_strmap_lookup_index(sc->map, item + sc->item_path_offset);
	git_strmap_delete_at(sc->map, mappos);

	if (sc->free_item)
		sc->free_item(sc->free_item_payload, item);

done:
	if (lock)
		git_sortedcache_unlock(sc);
	return error;
}

