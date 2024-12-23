#include "clar_libgit2.h"
#include "futils.h"
#include "pack.h"
#include "hash.h"
#include "iterator.h"
#include "vector.h"
#include "posix.h"
#include "hash.h"

static git_repository *_repo;
static git_revwalk *_revwalker;
static git_packbuilder *_packbuilder;
static git_indexer *_indexer;
static git_vector _commits;
static int _commits_is_initialized;
static git_indexer_progress _stats;

extern bool git_disable_pack_keep_file_checks;

void test_pack_packbuilder__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(p_chdir("testrepo.git"));
	cl_git_pass(git_revwalk_new(&_revwalker, _repo));
	cl_git_pass(git_packbuilder_new(&_packbuilder, _repo));
	cl_git_pass(git_vector_init(&_commits, 0, NULL));
	_commits_is_initialized = 1;
	memset(&_stats, 0, sizeof(_stats));
	p_fsync__cnt = 0;
}

void test_pack_packbuilder__cleanup(void)
{
	git_oid *o;
	unsigned int i;

	cl_git_pass(git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, 0));
	cl_git_pass(git_libgit2_opts(GIT_OPT_DISABLE_PACK_KEEP_FILE_CHECKS, false));

	if (_commits_is_initialized) {
		_commits_is_initialized = 0;
		git_vector_foreach(&_commits, i, o) {
			git__free(o);
		}
		git_vector_dispose(&_commits);
	}

	git_packbuilder_free(_packbuilder);
	_packbuilder = NULL;

	git_revwalk_free(_revwalker);
	_revwalker = NULL;

	git_indexer_free(_indexer);
	_indexer = NULL;

	cl_git_pass(p_chdir(".."));
	cl_git_sandbox_cleanup();
	_repo = NULL;
}

static void seed_packbuilder(void)
{
	git_oid oid, *o;
	unsigned int i;

	git_revwalk_sorting(_revwalker, GIT_SORT_TIME);
	cl_git_pass(git_revwalk_push_ref(_revwalker, "HEAD"));

	while (git_revwalk_next(&oid, _revwalker) == 0) {
		o = git__malloc(sizeof(git_oid));
		cl_assert(o != NULL);
		git_oid_cpy(o, &oid);
		cl_git_pass(git_vector_insert(&_commits, o));
	}

	git_vector_foreach(&_commits, i, o) {
		cl_git_pass(git_packbuilder_insert(_packbuilder, o, NULL));
	}

	git_vector_foreach(&_commits, i, o) {
		git_object *obj;
		cl_git_pass(git_object_lookup(&obj, _repo, o, GIT_OBJECT_COMMIT));
		cl_git_pass(git_packbuilder_insert_tree(_packbuilder,
					git_commit_tree_id((git_commit *)obj)));
		git_object_free(obj);
	}
}

static int feed_indexer(void *ptr, size_t len, void *payload)
{
	git_indexer_progress *stats = (git_indexer_progress *)payload;

	return git_indexer_append(_indexer, ptr, len, stats);
}

void test_pack_packbuilder__create_pack(void)
{
	git_indexer_progress stats;
	git_str buf = GIT_STR_INIT, path = GIT_STR_INIT;

	seed_packbuilder();

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_git_pass(git_indexer_new(&_indexer, ".", NULL));
#else
	cl_git_pass(git_indexer_new(&_indexer, ".", 0, NULL, NULL));
#endif

	cl_git_pass(git_packbuilder_foreach(_packbuilder, feed_indexer, &stats));
	cl_git_pass(git_indexer_commit(_indexer, &stats));

	git_str_printf(&path, "pack-%s.pack", git_indexer_name(_indexer));
	cl_assert(git_fs_path_exists(path.ptr));

	cl_git_pass(git_futils_readbuffer(&buf, git_str_cstr(&path)));
	cl_assert(buf.size > 256);

	git_str_dispose(&path);
	git_str_dispose(&buf);
}

void test_pack_packbuilder__get_name(void)
{
	seed_packbuilder();

	cl_git_pass(git_packbuilder_write(_packbuilder, ".", 0, NULL, NULL));
	cl_assert(git_packbuilder_name(_packbuilder) != NULL);
}

static void get_packfile_path(git_str *out, git_packbuilder *pb)
{
	git_str_puts(out, "pack-");
	git_str_puts(out, git_packbuilder_name(pb));
	git_str_puts(out, ".pack");
}

static void get_index_path(git_str *out, git_packbuilder *pb)
{
	git_str_puts(out, "pack-");
	git_str_puts(out, git_packbuilder_name(pb));
	git_str_puts(out, ".idx");
}

void test_pack_packbuilder__write_default_path(void)
{
	git_str idx = GIT_STR_INIT, pack = GIT_STR_INIT;

	seed_packbuilder();

	cl_git_pass(git_packbuilder_write(_packbuilder, NULL, 0, NULL, NULL));

	git_str_puts(&idx, "objects/pack/");
	get_index_path(&idx, _packbuilder);

	git_str_puts(&pack, "objects/pack/");
	get_packfile_path(&pack, _packbuilder);

	cl_assert(git_fs_path_exists(idx.ptr));
	cl_assert(git_fs_path_exists(pack.ptr));

	git_str_dispose(&idx);
	git_str_dispose(&pack);
}

static void test_write_pack_permission(mode_t given, mode_t expected)
{
	struct stat statbuf;
	mode_t mask, os_mask;
	git_str idx = GIT_STR_INIT, pack = GIT_STR_INIT;

	seed_packbuilder();

	cl_git_pass(git_packbuilder_write(_packbuilder, ".", given, NULL, NULL));

	/* Windows does not return group/user bits from stat,
	* files are never executable.
	*/
#ifdef GIT_WIN32
	os_mask = 0600;
#else
	os_mask = 0777;
#endif

	mask = p_umask(0);
	p_umask(mask);

	get_index_path(&idx, _packbuilder);
	get_packfile_path(&pack, _packbuilder);

	cl_git_pass(p_stat(idx.ptr, &statbuf));
	cl_assert_equal_i(statbuf.st_mode & os_mask, (expected & ~mask) & os_mask);

	cl_git_pass(p_stat(pack.ptr, &statbuf));
	cl_assert_equal_i(statbuf.st_mode & os_mask, (expected & ~mask) & os_mask);

	git_str_dispose(&idx);
	git_str_dispose(&pack);
}

void test_pack_packbuilder__permissions_standard(void)
{
	test_write_pack_permission(0, GIT_PACK_FILE_MODE);
}

void test_pack_packbuilder__permissions_readonly(void)
{
	test_write_pack_permission(0444, 0444);
}

void test_pack_packbuilder__permissions_readwrite(void)
{
	test_write_pack_permission(0666, 0666);
}

void test_pack_packbuilder__does_not_fsync_by_default(void)
{
	seed_packbuilder();
	cl_git_pass(git_packbuilder_write(_packbuilder, ".", 0666, NULL, NULL));
	cl_assert_equal_sz(0, p_fsync__cnt);
}

/* We fsync the packfile and index.  On non-Windows, we also fsync
 * the parent directories.
 */
#ifdef GIT_WIN32
static int expected_fsyncs = 2;
#else
static int expected_fsyncs = 4;
#endif

void test_pack_packbuilder__fsync_global_setting(void)
{
	cl_git_pass(git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, 1));
	p_fsync__cnt = 0;
	seed_packbuilder();
	cl_git_pass(git_packbuilder_write(_packbuilder, ".", 0666, NULL, NULL));
	cl_assert_equal_sz(expected_fsyncs, p_fsync__cnt);
}

void test_pack_packbuilder__fsync_repo_setting(void)
{
	cl_repo_set_bool(_repo, "core.fsyncObjectFiles", true);
	p_fsync__cnt = 0;
	seed_packbuilder();
	cl_git_pass(git_packbuilder_write(_packbuilder, ".", 0666, NULL, NULL));
	cl_assert_equal_sz(expected_fsyncs, p_fsync__cnt);
}

static int foreach_cb(void *buf, size_t len, void *payload)
{
	git_indexer *idx = (git_indexer *) payload;
	cl_git_pass(git_indexer_append(idx, buf, len, &_stats));
	return 0;
}

void test_pack_packbuilder__foreach(void)
{
	git_indexer *idx;

	seed_packbuilder();

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_git_pass(git_indexer_new(&idx, ".", NULL));
#else
	cl_git_pass(git_indexer_new(&idx, ".", 0, NULL, NULL));
#endif

	cl_git_pass(git_packbuilder_foreach(_packbuilder, foreach_cb, idx));
	cl_git_pass(git_indexer_commit(idx, &_stats));
	git_indexer_free(idx);
}

static int foreach_cancel_cb(void *buf, size_t len, void *payload)
{
	git_indexer *idx = (git_indexer *)payload;
	cl_git_pass(git_indexer_append(idx, buf, len, &_stats));
	return (_stats.total_objects > 2) ? -1111 : 0;
}

void test_pack_packbuilder__foreach_with_cancel(void)
{
	git_indexer *idx;

	seed_packbuilder();

#ifdef GIT_EXPERIMENTAL_SHA256
	cl_git_pass(git_indexer_new(&idx, ".", NULL));
#else
	cl_git_pass(git_indexer_new(&idx, ".", 0, NULL, NULL));
#endif

	cl_git_fail_with(
		git_packbuilder_foreach(_packbuilder, foreach_cancel_cb, idx), -1111);
	git_indexer_free(idx);
}

void test_pack_packbuilder__keep_file_check(void)
{
	assert(!git_disable_pack_keep_file_checks);
	cl_git_pass(git_libgit2_opts(GIT_OPT_DISABLE_PACK_KEEP_FILE_CHECKS, true));
	assert(git_disable_pack_keep_file_checks);
}
