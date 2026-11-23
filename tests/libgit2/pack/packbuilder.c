#include "clar_libgit2.h"
#include "futils.h"
#include "pack.h"
#include "pack-objects.h"
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

	cl_git_pass(git_indexer_new(&_indexer, ".", NULL));

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

	cl_git_pass(git_indexer_new(&idx, ".", NULL));

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

	cl_git_pass(git_indexer_new(&idx, ".", NULL));

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

static void oid_set_add(git_array_oid_t *set, const git_oid *id)
{
	git_oid *existing;
	size_t i;

	git_array_foreach(*set, i, existing) {
		if (git_oid_equal(existing, id))
			return;
	}

	cl_assert((existing = git_array_alloc(*set)) != NULL);
	git_oid_cpy(existing, id);
}

static bool oid_set_contains(git_array_oid_t *set, const git_oid *id)
{
	git_oid *existing;
	size_t i;

	git_array_foreach(*set, i, existing) {
		if (git_oid_equal(existing, id))
			return true;
	}

	return false;
}

static void add_tree_objects(git_array_oid_t *set, const git_oid *tree_id)
{
	git_tree *tree;
	size_t i;

	oid_set_add(set, tree_id);
	cl_git_pass(git_tree_lookup(&tree, _repo, tree_id));

	for (i = 0; i < git_tree_entrycount(tree); i++) {
		const git_tree_entry *entry = git_tree_entry_byindex(tree, i);

		if (git_tree_entry_type(entry) == GIT_OBJECT_TREE)
			add_tree_objects(set, git_tree_entry_id(entry));
		else if (git_tree_entry_type(entry) == GIT_OBJECT_BLOB)
			oid_set_add(set, git_tree_entry_id(entry));
	}

	git_tree_free(tree);
}

static void walk_from(git_revwalk *walk, const char **push, const char **hide)
{
	git_oid id;

	for (; *push; push++) {
		cl_git_pass(git_oid_from_string(&id, *push, GIT_OID_SHA1));
		cl_git_pass(git_revwalk_push(walk, &id));
	}

	for (; *hide; hide++) {
		cl_git_pass(git_oid_from_string(&id, *hide, GIT_OID_SHA1));
		cl_git_pass(git_revwalk_hide(walk, &id));
	}
}

/*
 * Check that inserting the walk from the `push` commits, hiding the
 * `hide` commits, inserts exactly the commits of the walk and the objects
 * they reference, except for the objects of the hidden parents of those
 * commits (the edges of the walk), which the other side has.
 */
static void assert_insert_walk(const char **push, const char **hide)
{
	git_array_oid_t commits = GIT_ARRAY_INIT, objects = GIT_ARRAY_INIT,
		edges = GIT_ARRAY_INIT, expected = GIT_ARRAY_INIT;
	git_revwalk *walk;
	git_commit *commit;
	git_oid id, *object;
	size_t i, j;

	/* What to expect, worked out with a separate walk */
	cl_git_pass(git_revwalk_new(&walk, _repo));
	walk_from(walk, push, hide);

	while (git_revwalk_next(&id, walk) == 0)
		oid_set_add(&commits, &id);

	git_revwalk_free(walk);

	git_array_foreach(commits, i, object) {
		cl_git_pass(git_commit_lookup(&commit, _repo, object));
		oid_set_add(&objects, object);
		add_tree_objects(&objects, git_commit_tree_id(commit));

		for (j = 0; j < git_commit_parentcount(commit); j++) {
			const git_oid *parent_id = git_commit_parent_id(commit, j);
			git_commit *parent;

			if (oid_set_contains(&commits, parent_id))
				continue;

			cl_git_pass(git_commit_lookup(&parent, _repo, parent_id));
			add_tree_objects(&edges, git_commit_tree_id(parent));
			git_commit_free(parent);
		}

		git_commit_free(commit);
	}

	git_array_foreach(objects, i, object) {
		if (!oid_set_contains(&edges, object))
			oid_set_add(&expected, object);
	}

	/* What gets inserted */
	walk_from(_revwalker, push, hide);
	cl_git_pass(git_packbuilder_insert_walk(_packbuilder, _revwalker));

	cl_assert_equal_sz(git_array_size(expected), _packbuilder->nr_objects);
	for (i = 0; i < _packbuilder->nr_objects; i++)
		cl_assert(oid_set_contains(&expected, &_packbuilder->object_list[i].id));

	git_array_dispose(commits);
	git_array_dispose(objects);
	git_array_dispose(edges);
	git_array_dispose(expected);
}

void test_pack_packbuilder__insert_walk_omits_objects_of_hidden_parents(void)
{
	/* "Add some files into subdirectories", on top of "branch commit one" */
	const char *push[] = { "763d71aadf09a7951596c9746c024e7eece7c7af", NULL };
	const char *hide[] = { "c47800c7266a2be04c571c04d5a6614691ea99bd", NULL };

	assert_insert_walk(push, hide);
}

void test_pack_packbuilder__insert_walk_with_merges(void)
{
	/*
	 * The walk includes the merge be3563a and, from the merged branch,
	 * c47800c: the hidden 9fd738e and 5b5b025 are their parents.
	 */
	const char *push[] = { "a65fedf39aefe402d3bb6e24df4d4f5fe4547750", NULL };
	const char *hide[] = { "9fd738e8f7967c078dceed8190330fc8648ee56a", NULL };

	assert_insert_walk(push, hide);
}

void test_pack_packbuilder__insert_walk_reads_only_hidden_parents(void)
{
	/*
	 * Hidden commits that aren't parents of the inserted commits have no
	 * influence on what gets inserted, and aren't read beyond what the
	 * walk needs: here, one of them refers to a tree that doesn't exist.
	 * (There may be many of them, e.g. one for every reference a remote
	 * advertises, with large trees.)
	 */
	const char *broken_commit =
		"tree 0123456789012345678901234567890123456789\n"
		"author A U Thor <author@example.com> 1700000000 +0000\n"
		"committer A U Thor <author@example.com> 1700000000 +0000\n"
		"\n"
		"refers to a tree that doesn't exist\n";
	char broken_id[GIT_OID_SHA1_HEXSIZE + 1];
	const char *push[] = { "763d71aadf09a7951596c9746c024e7eece7c7af", NULL };
	const char *hide[] = {
		"c47800c7266a2be04c571c04d5a6614691ea99bd",
		/* "Test commit 2", on another branch */
		"e90810b8df3e80c413d903f631643c716887138d",
		broken_id,
		NULL
	};
	git_odb *odb;
	git_oid id;

	cl_git_pass(git_repository_odb(&odb, _repo));
	cl_git_pass(git_odb_write(&id, odb, broken_commit,
		strlen(broken_commit), GIT_OBJECT_COMMIT));
	git_odb_free(odb);
	git_oid_tostr(broken_id, sizeof(broken_id), &id);

	assert_insert_walk(push, hide);
}

