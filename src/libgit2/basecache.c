#include "basecache.h"

#include "sizemap.h"

#include "git2_util.h"



/* todo */
struct object_data {
size_t len;
git_object_t type;
unsigned char data[GIT_FLEX_ARRAY];
};



int basecache_init(struct basecache *bc)
{
	memset(bc, 0, sizeof(struct basecache));

	if (git_sizemap_new(&bc->map) < 0 /*||
	    git_rwlock_init(&bc->lock) < 0*/)
		return -1;

	bc->size = SIZE_MAX;

	return 0;
}

struct object_data *basecache_get(
	struct basecache *bc,
	git_object_size_t position)
{
	struct object_data *out = NULL;
	basecache_entry *entry;

	if (/*git_rwlock_rdlock(&bc->lock) < 0 || */
	    (entry = git_sizemap_get(bc->map, position)) == NULL) {
		goto done;
	}

	/* promote this entry to newest */
	if (entry->prev)
		entry->prev->next = entry->next;

	if (entry->next)
		entry->next->prev = entry->prev;

	entry->prev = bc->newest;
	entry->next = NULL;
	bc->newest = entry;

	out = entry->data;

done:
/*	git_rwlock_rdunlock(&bc->lock); */
	return out;
}

/* Called under write lock */
GIT_INLINE(int) reserve_space(struct basecache *bc, size_t needed)
{
	basecache_entry *old;

	return 0;

	while (bc->oldest && (bc->size - bc->used) < needed) {
		old = bc->oldest;
		bc->oldest = old->next;

		GIT_ASSERT(bc->used >= old->size);

		if (old->prev)
			old->prev->next = old->next;

		if (old->next)
			old->next->prev = old->prev;

		git_sizemap_delete(bc->map, old->position);

		bc->used -= old->size;
		git__free(old);
	}

	GIT_ASSERT(bc->oldest != NULL || bc->used > 0);

	return 0;
}

int basecache_put(
	struct basecache *bc,
	git_object_size_t position,
	struct object_data *data)
{
	basecache_entry *new;
	int error = 0;

	/* TODO: pool? */
	new = git__malloc(sizeof(basecache_entry));
	GIT_ERROR_CHECK_ALLOC(new);

/*
	if (git_rwlock_wrlock(&bc->lock) < 0) {
		git__free(new);
		return -1;
	}
	*/

	GIT_ASSERT_WITH_CLEANUP(bc->used <= bc->size, {
		error = -1;
		goto done;
	});

	/* TODO: cache size limits */
	if (data->len > bc->size)
		goto done;

	if (reserve_space(bc, data->len) < 0 ||
	    git_sizemap_set(bc->map, position, new) < 0) {
		error = -1;
		goto done;
	}

	new->data = data;
	new->position = position;
	new->prev = bc->newest;
	new->next = NULL;
	bc->newest = new;

	if (!bc->oldest)
		bc->oldest = new;

	bc->used += data->len;

done:
	if (error < 0)
		git__free(new);

/*	git_rwlock_wrunlock(&bc->lock); */
	return error;
}

void basecache_dispose(struct basecache *bc)
{
	basecache_entry *entry, *dispose;

	if (/*git_rwlock_wrlock(&bc->lock) */ 0 == 0) {
		entry = bc->oldest;

		while (entry != NULL) {
			dispose = entry;
			entry = entry->next;

			git__free(dispose);
		}

/*
		git_rwlock_wrunlock(&bc->lock);
		git_rwlock_free(&bc->lock);
		*/
	}
}
