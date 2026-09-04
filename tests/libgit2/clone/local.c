#include "clar_libgit2.h"

#include "git2/clone.h"
#include "git2/index.h"
#include "git2/sparse.h"
#include "clone.h"
#include "path.h"
#include "posix.h"
#include "futils.h"

static int file_url(git_str *buf, const char *host, const char *path)
{
	if (path[0] == '/')
		path++;

	git_str_clear(buf);
	return git_str_printf(buf, "file://%s/%s", host, path);
}

#ifdef GIT_WIN32
static int git_style_unc_path(git_str *buf, const char *host, const char *path)
{
	git_str_clear(buf);

	if (host)
		git_str_printf(buf, "//%s/", host);

	if (path[0] == '/')
		path++;

	if (git__isalpha(path[0]) && path[1] == ':' && path[2] == '/') {
		git_str_printf(buf, "%c$/", path[0]);
		path += 3;
	}

	git_str_puts(buf, path);

	return git_str_oom(buf) ? -1 : 0;
}

static int unc_path(git_str *buf, const char *host, const char *path)
{
	char *c;

	if (git_style_unc_path(buf, host, path) < 0)
		return -1;

	for (c = buf->ptr; *c; c++)
		if (*c == '/')
			*c = '\\';

	return 0;
}
#endif

void test_clone_local__should_clone_local(void)
{
	git_str buf = GIT_STR_INIT;
	bool local;

	/* we use a fixture path because it needs to exist for us to want to clone */
	const char *path = cl_fixture("testrepo.git");

	/* empty string */
	cl_git_pass(file_url(&buf, "", path));
	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_NO_LOCAL));
	cl_assert_equal_i(false, local);

	/* localhost is special */
	cl_git_pass(file_url(&buf, "localhost", path));
	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_NO_LOCAL));
	cl_assert_equal_i(false, local);

	/* a remote host */
	cl_git_pass(file_url(&buf, "other-host.mycompany.com", path));

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_NO_LOCAL));
	cl_assert_equal_i(false, local);

	/* Ensure that file:/// urls are percent decoded: .git == %2e%67%69%74 */
	cl_git_pass(file_url(&buf, "", path));
	git_str_shorten(&buf, 4);
	cl_git_pass(git_str_puts(&buf, "%2e%67%69%74"));

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(false, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, buf.ptr, GIT_CLONE_NO_LOCAL));
	cl_assert_equal_i(false, local);

	/* a local path on disk */
	cl_git_pass(git_clone__should_clone_local(&local, path, GIT_CLONE_LOCAL_AUTO));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, path, GIT_CLONE_LOCAL));

	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, path, GIT_CLONE_LOCAL_NO_LINKS));
	cl_assert_equal_i(true, local);

	cl_git_pass(git_clone__should_clone_local(&local, path, GIT_CLONE_NO_LOCAL));
	cl_assert_equal_i(false, local);

	git_str_dispose(&buf);
}

void test_clone_local__hardlinks(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_str buf = GIT_STR_INIT;
	struct stat st;

	/*
	 * In this first clone, we just copy over, since the temp dir
	 * will often be in a different filesystem, so we cannot
	 * link. It also allows us to control the number of links
	 */
	opts.bare = true;
	opts.local = GIT_CLONE_LOCAL_NO_LINKS;
	cl_git_pass(git_clone(&repo, cl_fixture("testrepo.git"), "./clone.git", &opts));
	git_repository_free(repo);

	/* This second clone is in the same filesystem, so we can hardlink */

	opts.local = GIT_CLONE_LOCAL;
	cl_git_pass(git_clone(&repo, cl_git_path_url("clone.git"), "./clone2.git", &opts));

#ifndef GIT_WIN32
	git_str_clear(&buf);
	cl_git_pass(git_str_join_n(&buf, '/', 4, git_repository_path(repo), "objects", "08", "b041783f40edfe12bb406c9c9a8a040177c125"));

	cl_git_pass(p_stat(buf.ptr, &st));
	cl_assert_equal_i(2, st.st_nlink);
#endif

	git_repository_free(repo);
	git_str_clear(&buf);

	opts.local = GIT_CLONE_LOCAL_NO_LINKS;
	cl_git_pass(git_clone(&repo, cl_git_path_url("clone.git"), "./clone3.git", &opts));

	git_str_clear(&buf);
	cl_git_pass(git_str_join_n(&buf, '/', 4, git_repository_path(repo), "objects", "08", "b041783f40edfe12bb406c9c9a8a040177c125"));

	cl_git_pass(p_stat(buf.ptr, &st));
	cl_assert_equal_i(1, st.st_nlink);

	git_repository_free(repo);

	/* this one should automatically use links */
	cl_git_pass(git_clone(&repo, "./clone.git", "./clone4.git", NULL));

#ifndef GIT_WIN32
	git_str_clear(&buf);
	cl_git_pass(git_str_join_n(&buf, '/', 4, git_repository_path(repo), "objects", "08", "b041783f40edfe12bb406c9c9a8a040177c125"));

	cl_git_pass(p_stat(buf.ptr, &st));
	cl_assert_equal_i(3, st.st_nlink);
#endif

	git_str_dispose(&buf);
	git_repository_free(repo);

	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r("./clone2.git", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r("./clone3.git", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r("./clone4.git", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_clone_local__standard_unc_paths_are_written_git_style(void)
{
#ifdef GIT_WIN32
	git_repository *repo;
	git_remote *remote;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_str unc = GIT_STR_INIT, git_unc = GIT_STR_INIT;

	/* we use a fixture path because it needs to exist for us to want to clone */
	const char *path = cl_fixture("testrepo.git");

	cl_git_pass(unc_path(&unc, "localhost", path));
	cl_git_pass(git_style_unc_path(&git_unc, "localhost", path));

	cl_git_pass(git_clone(&repo, unc.ptr, "./clone.git", &opts));
	cl_git_pass(git_remote_lookup(&remote, repo, "origin"));

	cl_assert_equal_s(git_unc.ptr, git_remote_url(remote));

	git_remote_free(remote);
	git_repository_free(repo);
	git_str_dispose(&unc);
	git_str_dispose(&git_unc);

	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
#endif
}

void test_clone_local__git_style_unc_paths(void)
{
#ifdef GIT_WIN32
	git_repository *repo;
	git_remote *remote;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_str git_unc = GIT_STR_INIT;

	/* we use a fixture path because it needs to exist for us to want to clone */
	const char *path = cl_fixture("testrepo.git");

	cl_git_pass(git_style_unc_path(&git_unc, "localhost", path));

	cl_git_pass(git_clone(&repo, git_unc.ptr, "./clone.git", &opts));
	cl_git_pass(git_remote_lookup(&remote, repo, "origin"));

	cl_assert_equal_s(git_unc.ptr, git_remote_url(remote));

	git_remote_free(remote);
	git_repository_free(repo);
	git_str_dispose(&git_unc);

	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
#endif
}

void test_clone_local__shallow_fails(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.depth = 4;

	cl_git_fail_with(GIT_ENOTSUPPORTED, git_clone(&repo, cl_fixture("testrepo.git"), "./clone.git", &opts));
}

void test_clone_local__filtered_clone_reaches_transport(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "blob:none";

	cl_git_fail_with(
	        GIT_ENOTSUPPORTED, git_clone(
	                                   &repo, cl_fixture("testrepo.git"),
	                                   "./clone.git", &opts));
}

void test_clone_local__filtered_sparse_clone_reaches_transport(void)
{
	char *directories[] = { "source" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "blob:none";
	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;

	cl_git_fail_with(
		GIT_ENOTSUPPORTED,
		git_clone(&repo, cl_fixture("testrepo.git"), "./clone.git", &opts));
}

void test_clone_local__blob_limit_filtered_sparse_clone_reaches_transport(void)
{
	char *directories[] = { "source" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "blob:limit=1k";
	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;

	cl_git_fail_with(
	        GIT_ENOTSUPPORTED, git_clone(
	                                   &repo, cl_fixture("testrepo.git"),
	                                   "./clone.git", &opts));
}

void test_clone_local__tree_depth_filtered_sparse_clone_reaches_transport(void)
{
	char *directories[] = { "source" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "tree:2";
	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;

	cl_git_fail_with(
	        GIT_ENOTSUPPORTED, git_clone(
	                                   &repo, cl_fixture("testrepo.git"),
	                                   "./clone.git", &opts));
}

void test_clone_local__rejects_invalid_blob_limit_filter(void)
{
	char *directories[] = { "source" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "blob:limit=1x";
	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;

	cl_git_fail_with(
	        GIT_EINVALIDSPEC, git_clone(
	                                  &repo, cl_fixture("testrepo.git"),
	                                  "./clone.git", &opts));
}

void test_clone_local__rejects_invalid_tree_depth_filter(void)
{
	char *directories[] = { "source" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.fetch_opts.filter_spec = "tree:";
	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;

	cl_git_fail_with(
	        GIT_EINVALIDSPEC, git_clone(
	                                  &repo, cl_fixture("testrepo.git"),
	                                  "./clone.git", &opts));
}

void test_clone_local__sparse_checkout(void)
{
	char *directories[] = { "selected" };
	git_strarray sparse_directories = {
		directories,
		ARRAY_SIZE(directories),
	};
	git_strarray root_only = { NULL, 0 };
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_index *index;
	git_repository *source, *clone, *full;
	int sparse_enabled;

	cl_git_pass(git_repository_init(&source, "./source", 0));
	cl_git_mkfile("./source/root.txt", "root\n");
	cl_git_pass(p_mkdir("./source/selected", 0777));
	cl_git_mkfile("./source/selected/keep.txt", "keep\n");
	cl_git_pass(p_mkdir("./source/excluded", 0777));
	cl_git_mkfile("./source/excluded/drop.txt", "drop\n");

	cl_git_pass(git_repository_index(&index, source));
	cl_git_pass(git_index_add_all(index, NULL, 0, NULL, NULL));
	cl_git_pass(git_index_write(index));
	cl_repo_commit_from_index(NULL, source, NULL, 0, "initial commit");
	cl_git_pass(git_clone(&full, "./source", "./full", NULL));

	cl_git_pass(git_sparse_checkout_apply(
		full, &sparse_directories, NULL));
	cl_assert(!git_fs_path_isfile("./full/excluded/drop.txt"));

	cl_git_pass(p_mkdir("./full/excluded", 0777));
	cl_git_mkfile("./full/excluded/drop.txt", "modified\n");
	cl_git_pass(git_sparse_checkout_apply(full, &root_only, NULL));
	cl_assert(git_fs_path_isfile("./full/excluded/drop.txt"));

	opts.sparse_checkout_directories = sparse_directories;
	opts.sparse_checkout = 1;
	cl_git_pass(git_clone(&clone, "./source", "./clone", &opts));

	cl_assert(git_fs_path_isfile("./clone/root.txt"));
	cl_assert(git_fs_path_isfile("./clone/selected/keep.txt"));
	cl_assert(!git_fs_path_isfile("./clone/excluded/drop.txt"));
	cl_git_pass(git_sparse_checkout_disable(clone, NULL));
	cl_git_pass(git_sparse_checkout_is_enabled(&sparse_enabled, clone));
	cl_assert(!sparse_enabled);
	cl_assert(git_fs_path_isfile("./clone/excluded/drop.txt"));

	git_repository_free(clone);
	opts.sparse_checkout_directories.strings = NULL;
	opts.sparse_checkout_directories.count = 0;
	cl_git_pass(git_clone(&clone, "./source", "./root-only", &opts));
	cl_assert(git_fs_path_isfile("./root-only/root.txt"));
	cl_assert(!git_fs_path_isfile("./root-only/selected/keep.txt"));
	cl_assert(!git_fs_path_isfile("./root-only/excluded/drop.txt"));

	git_index_free(index);
	git_repository_free(clone);
	git_repository_free(full);
	git_repository_free(source);
	cl_git_pass(
	        git_futils_rmdir_r("./clone", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(git_futils_rmdir_r(
	        "./root-only", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(
	        git_futils_rmdir_r("./full", NULL, GIT_RMDIR_REMOVE_FILES));
	cl_git_pass(
	        git_futils_rmdir_r("./source", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_clone_local__sha256_via_no_local(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	/*
	 * file:// URL + GIT_CLONE_NO_LOCAL -> remote code path
	 * Should correctly propagate the object format
	 */
	opts.bare = true;
	opts.local = GIT_CLONE_NO_LOCAL;
	cl_git_pass(git_clone(&repo, cl_git_fixture_url("testrepo_256.git"), "./clone.git", &opts));

	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(repo));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_clone_local__sha256_object_format_is_propagated(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	/*
	 * file:// URL + GIT_CLONE_LOCAL -> local code path with hardlinks
	 * Should correctly propagate the object format
	 */
	opts.bare = true;
	opts.local = GIT_CLONE_LOCAL;
	cl_git_pass(git_clone(&repo, cl_git_fixture_url("testrepo_256.git"), "./clone.git", &opts));

	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(repo));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
}

void test_clone_local__sha256_no_links_object_format_is_propagated(void)
{
	git_repository *repo;
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	/*
	 * file:// URL + GIT_CLONE_LOCAL_NO_LINKS -> local code path with copy
	 * Should correctly propagate the object format
	 */
	opts.bare = true;
	opts.local = GIT_CLONE_LOCAL_NO_LINKS;
	cl_git_pass(git_clone(&repo, cl_git_fixture_url("testrepo_256.git"), "./clone.git", &opts));

	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(repo));

	git_repository_free(repo);
	cl_git_pass(git_futils_rmdir_r("./clone.git", NULL, GIT_RMDIR_REMOVE_FILES));
}
