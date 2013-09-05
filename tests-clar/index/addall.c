#include "clar_libgit2.h"
#include "../status/status_helpers.h"
#include "posix.h"
#include "fileops.h"

git_repository *g_repo = NULL;

void test_index_addall__initialize(void)
{
}

void test_index_addall__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
}

#define STATUS_INDEX_FLAGS \
	(GIT_STATUS_INDEX_NEW | GIT_STATUS_INDEX_MODIFIED | \
	 GIT_STATUS_INDEX_DELETED | GIT_STATUS_INDEX_RENAMED | \
	 GIT_STATUS_INDEX_TYPECHANGE)

#define STATUS_WT_FLAGS \
	(GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED | \
	 GIT_STATUS_WT_DELETED | GIT_STATUS_WT_TYPECHANGE | \
	 GIT_STATUS_WT_RENAMED)

typedef struct {
	size_t index_adds;
	size_t index_dels;
	size_t index_mods;
	size_t wt_adds;
	size_t wt_dels;
	size_t wt_mods;
	size_t ignores;
} index_status_counts;

static int index_status_cb(
	const char *path, unsigned int status_flags, void *payload)
{
	index_status_counts *vals = payload;

	/* cb_status__print(path, status_flags, NULL); */

	GIT_UNUSED(path);

	if (status_flags & GIT_STATUS_INDEX_NEW)
		vals->index_adds++;
	if (status_flags & GIT_STATUS_INDEX_MODIFIED)
		vals->index_mods++;
	if (status_flags & GIT_STATUS_INDEX_DELETED)
		vals->index_dels++;
	if (status_flags & GIT_STATUS_INDEX_TYPECHANGE)
		vals->index_mods++;

	if (status_flags & GIT_STATUS_WT_NEW)
		vals->wt_adds++;
	if (status_flags & GIT_STATUS_WT_MODIFIED)
		vals->wt_mods++;
	if (status_flags & GIT_STATUS_WT_DELETED)
		vals->wt_dels++;
	if (status_flags & GIT_STATUS_WT_TYPECHANGE)
		vals->wt_mods++;

	if (status_flags & GIT_STATUS_IGNORED)
		vals->ignores++;

	return 0;
}

static void check_status(
	git_repository *repo,
	size_t index_adds, size_t index_dels, size_t index_mods,
	size_t wt_adds, size_t wt_dels, size_t wt_mods, size_t ignores)
{
	index_status_counts vals;

	memset(&vals, 0, sizeof(vals));

	cl_git_pass(git_status_foreach(repo, index_status_cb, &vals));

	cl_assert_equal_sz(index_adds, vals.index_adds);
	cl_assert_equal_sz(index_dels, vals.index_dels);
	cl_assert_equal_sz(index_mods, vals.index_mods);
	cl_assert_equal_sz(wt_adds, vals.wt_adds);
	cl_assert_equal_sz(wt_dels, vals.wt_dels);
	cl_assert_equal_sz(wt_mods, vals.wt_mods);
	cl_assert_equal_sz(ignores, vals.ignores);
}

static void check_stat_data(git_index *index, const char *path, bool match)
{
	const git_index_entry *entry;
	struct stat st;

	cl_must_pass(p_lstat(path, &st));

	/* skip repo base dir name */
	while (*path != '/')
		++path;
	++path;

	entry = git_index_get_bypath(index, path, 0);
	cl_assert(entry);

	if (match) {
		cl_assert(st.st_ctime == entry->ctime.seconds);
		cl_assert(st.st_mtime == entry->mtime.seconds);
		cl_assert(st.st_size == entry->file_size);
		cl_assert(st.st_uid  == entry->uid);
		cl_assert(st.st_gid  == entry->gid);
		cl_assert_equal_i_fmt(
			GIT_MODE_TYPE(st.st_mode), GIT_MODE_TYPE(entry->mode), "%07o");
		cl_assert_equal_b(
			GIT_PERMS_IS_EXEC(st.st_mode), GIT_PERMS_IS_EXEC(entry->mode));
	} else {
		/* most things will still match */
		cl_assert(st.st_size != entry->file_size);
		/* would check mtime, but with second resolution it won't work :( */
	}
}

static void commit_index_to_head(
	git_repository *repo,
	const char *commit_message)
{
	git_index *index;
	git_oid tree_id, commit_id;
	git_tree *tree;
	git_signature *sig;
	git_commit *parent = NULL;

	git_revparse_single((git_object **)&parent, repo, "HEAD");
	/* it is okay if looking up the HEAD fails */

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_write_tree(&tree_id, index));
	cl_git_pass(git_index_write(index)); /* not needed, but might as well */
	git_index_free(index);

	cl_git_pass(git_tree_lookup(&tree, repo, &tree_id));

	cl_git_pass(git_signature_now(&sig, "Testy McTester", "tt@tester.test"));

	cl_git_pass(git_commit_create_v(
		&commit_id, repo, "HEAD", sig, sig,
		NULL, commit_message, tree, parent ? 1 : 0, parent));

	git_commit_free(parent);
	git_tree_free(tree);
	git_signature_free(sig);
}

void test_index_addall__repo_lifecycle(void)
{
	int error;
	git_index *index;
	git_strarray paths = { NULL, 0 };
	char *strs[1];

	cl_git_pass(git_repository_init(&g_repo, "addall", false));
	check_status(g_repo, 0, 0, 0, 0, 0, 0, 0);

	cl_git_pass(git_repository_index(&index, g_repo));

	cl_git_mkfile("addall/file.foo", "a file");
	check_status(g_repo, 0, 0, 0, 1, 0, 0, 0);

	cl_git_mkfile("addall/.gitignore", "*.foo\n");
	check_status(g_repo, 0, 0, 0, 1, 0, 0, 1);

	cl_git_mkfile("addall/file.bar", "another file");
	check_status(g_repo, 0, 0, 0, 2, 0, 0, 1);

	strs[0] = "file.*";
	paths.strings = strs;
	paths.count   = 1;

	cl_git_pass(git_index_add_all(index, &paths, 0, NULL, NULL));
	check_stat_data(index, "addall/file.bar", true);
	check_status(g_repo, 1, 0, 0, 1, 0, 0, 1);

	cl_git_rewritefile("addall/file.bar", "new content for file");
	check_stat_data(index, "addall/file.bar", false);
	check_status(g_repo, 1, 0, 0, 1, 0, 1, 1);

	cl_git_mkfile("addall/file.zzz", "yet another one");
	cl_git_mkfile("addall/other.zzz", "yet another one");
	cl_git_mkfile("addall/more.zzz", "yet another one");
	check_status(g_repo, 1, 0, 0, 4, 0, 1, 1);

	cl_git_pass(git_index_update_all(index, NULL, NULL, NULL));
	check_stat_data(index, "addall/file.bar", true);
	check_status(g_repo, 1, 0, 0, 4, 0, 0, 1);

	cl_git_pass(git_index_add_all(index, &paths, 0, NULL, NULL));
	check_stat_data(index, "addall/file.zzz", true);
	check_status(g_repo, 2, 0, 0, 3, 0, 0, 1);

	commit_index_to_head(g_repo, "first commit");
	check_status(g_repo, 0, 0, 0, 3, 0, 0, 1);

	/* attempt to add an ignored file - does nothing */
	strs[0] = "file.foo";
	cl_git_pass(git_index_add_all(index, &paths, 0, NULL, NULL));
	check_status(g_repo, 0, 0, 0, 3, 0, 0, 1);

	/* add with check - should generate error */
	error = git_index_add_all(
		index, &paths, GIT_INDEX_ADD_CHECK_PATHSPEC, NULL, NULL);
	cl_assert_equal_i(GIT_EINVALIDSPEC, error);
	check_status(g_repo, 0, 0, 0, 3, 0, 0, 1);

	/* add with force - should allow */
	cl_git_pass(git_index_add_all(
		index, &paths, GIT_INDEX_ADD_FORCE, NULL, NULL));
	check_stat_data(index, "addall/file.foo", true);
	check_status(g_repo, 1, 0, 0, 3, 0, 0, 0);

	/* now it's in the index, so regular add should work */
	cl_git_rewritefile("addall/file.foo", "new content for file");
	check_stat_data(index, "addall/file.foo", false);
	check_status(g_repo, 1, 0, 0, 3, 0, 1, 0);

	cl_git_pass(git_index_add_all(index, &paths, 0, NULL, NULL));
	check_stat_data(index, "addall/file.foo", true);
	check_status(g_repo, 1, 0, 0, 3, 0, 0, 0);

	cl_git_pass(git_index_add_bypath(index, "more.zzz"));
	check_stat_data(index, "addall/more.zzz", true);
	check_status(g_repo, 2, 0, 0, 2, 0, 0, 0);

	cl_git_rewritefile("addall/file.zzz", "new content for file");
	check_status(g_repo, 2, 0, 0, 2, 0, 1, 0);

	cl_git_pass(git_index_add_bypath(index, "file.zzz"));
	check_stat_data(index, "addall/file.zzz", true);
	check_status(g_repo, 2, 0, 1, 2, 0, 0, 0);

	strs[0] = "*.zzz";
	cl_git_pass(git_index_remove_all(index, &paths, NULL, NULL));
	check_status(g_repo, 1, 1, 0, 4, 0, 0, 0);

	cl_git_pass(git_index_add_bypath(index, "file.zzz"));
	check_status(g_repo, 1, 0, 1, 3, 0, 0, 0);

	commit_index_to_head(g_repo, "second commit");
	check_status(g_repo, 0, 0, 0, 3, 0, 0, 0);

	cl_must_pass(p_unlink("addall/file.zzz"));
	check_status(g_repo, 0, 0, 0, 3, 1, 0, 0);

	/* update_all should be able to remove entries */
	cl_git_pass(git_index_update_all(index, NULL, NULL, NULL));
	check_status(g_repo, 0, 1, 0, 3, 0, 0, 0);

	strs[0] = "*";
	cl_git_pass(git_index_add_all(index, &paths, 0, NULL, NULL));
	check_status(g_repo, 3, 1, 0, 0, 0, 0, 0);

	/* must be able to remove at any position while still updating other files */
	cl_must_pass(p_unlink("addall/.gitignore"));
	cl_git_rewritefile("addall/file.zzz", "reconstructed file");
	cl_git_rewritefile("addall/more.zzz", "altered file reality");
	check_status(g_repo, 3, 1, 0, 1, 1, 1, 0);

	cl_git_pass(git_index_update_all(index, NULL, NULL, NULL));
	check_status(g_repo, 2, 1, 0, 1, 0, 0, 0);
	/* this behavior actually matches 'git add -u' where "file.zzz" has
	 * been removed from the index, so when you go to update, even though
	 * it exists in the HEAD, it is not re-added to the index, leaving it
	 * as a DELETE when comparing HEAD to index and as an ADD comparing
	 * index to worktree
	 */

	git_index_free(index);
}
