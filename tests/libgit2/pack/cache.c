#include "clar_libgit2.h"
#include "mwindow.h"
#include "pack.h"

/* A copy of the pack, so that no other test has it open already. */
#define PACK_IDX "testrepo.git/objects/pack/pack-a81e489679b7d3418f9ab594bda8ceb37dd4c695.idx"

static struct git_pack_file *pack;
static git_array_t(git_oid) ids;

void test_pack_cache__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");
}

void test_pack_cache__cleanup(void)
{
	if (pack) {
		git_mwindow_put_pack(pack);
		pack = NULL;
	}
	git_array_clear(ids);
	cl_fixture_cleanup("testrepo.git");
}

static int collect_id(const git_oid *id, void *payload)
{
	git_oid *out = git_array_alloc(ids);

	GIT_UNUSED(payload);
	GIT_ERROR_CHECK_ALLOC(out);
	git_oid_cpy(out, id);
	return 0;
}

static void open_pack(void)
{
	cl_git_pass(git_mwindow_get_pack(&pack, PACK_IDX, GIT_OID_SHA1));
	git_array_clear(ids);
	cl_git_pass(git_pack_foreach_entry(pack, collect_id, NULL));
	cl_assert(git_array_size(ids) > 0);
}

static void close_pack(void)
{
	git_mwindow_put_pack(pack);
	pack = NULL;
}

static void read_all(void)
{
	size_t i;

	for (i = 0; i < git_array_size(ids); i++) {
		struct git_pack_entry e;
		git_rawobj raw;

		cl_git_pass(git_pack_entry_find(&e, pack, git_array_get(ids, i), GIT_OID_SHA1_HEXSIZE));
		cl_git_pass(git_packfile_unpack(&raw, pack, &e.offset));
		git__free(raw.data);
	}
}

/*
 * Every entry in the map is on the LRU list once, no entry is still in
 * use, and the cache counts exactly the memory its entries hold.
 */
static void assert_cache_consistent(git_pack_cache *cache)
{
	git_pack_cache_entry *e, *prev = NULL;
	size_t count = 0, used = 0;

	for (e = cache->head; e; prev = e, e = e->next) {
		cl_assert(e->prev == prev);
		cl_assert_equal_i(0, git_atomic32_get(&e->refcount));
		used += e->raw.len;
		count++;
	}

	cl_assert(cache->tail == prev);
	cl_assert_equal_sz(cache->entries.size, count);
	cl_assert_equal_sz(cache->memory_used, used);
	cl_assert(cache->memory_used <= cache->memory_limit);
}

void test_pack_cache__holds_bases(void)
{
	open_pack();
	read_all();

	cl_assert(pack->bases.memory_used > 0);
	assert_cache_consistent(&pack->bases);
}

void test_pack_cache__evicts_to_stay_within_limit(void)
{
	size_t unlimited;

	/* how much the cache holds when nothing has to be evicted */
	open_pack();
	read_all();
	unlimited = pack->bases.memory_used;
	close_pack();

	/* half of that can only be kept by evicting */
	open_pack();
	pack->bases.memory_limit = unlimited / 2;
	read_all();
	read_all();

	cl_assert(pack->bases.memory_used > 0);
	assert_cache_consistent(&pack->bases);
}
