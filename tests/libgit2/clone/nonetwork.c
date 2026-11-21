#include "clar_libgit2.h"

#include "git2/clone.h"
#include "../submodule/submodule_helpers.h"
#include "remote.h"
#include "futils.h"
#include "repository.h"
#include "index.h"

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_clone_options g_options;
static git_repository *g_repo;
static git_reference* g_ref;
static git_remote* g_remote;

void test_clone_nonetwork__initialize(void)
{
	git_checkout_options dummy_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_fetch_options dummy_fetch = GIT_FETCH_OPTIONS_INIT;

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.checkout_opts = dummy_opts;
	g_options.fetch_opts = dummy_fetch;
}

void test_clone_nonetwork__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	if (g_ref) {
		git_reference_free(g_ref);
		g_ref = NULL;
	}

	if (g_remote) {
		git_remote_free(g_remote);
		g_remote = NULL;
	}

	cl_fixture_cleanup("./foo");
}

void test_clone_nonetwork__bad_urls(void)
{
	/* Clone should clean up the mess if the URL isn't a git repository */
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_fs_path_exists("./foo"));
	g_options.bare = true;
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_fs_path_exists("./foo"));

	cl_git_fail(git_clone(&g_repo, "git://example.com:asdf", "./foo", &g_options));
	cl_git_fail(git_clone(&g_repo, "https://example.com:asdf/foo", "./foo", &g_options));
	cl_git_fail(git_clone(&g_repo, "git://github.com/git://github.com/foo/bar.git.git",
				"./foo", &g_options));
	cl_git_fail(git_clone(&g_repo, "arrbee:my/bad:password@github.com:1111/strange:words.git",
				"./foo", &g_options));
}

void test_clone_nonetwork__rejects_unknown_url_scheme(void)
{
	cl_git_fail(git_clone(&g_repo, "foo://example.com", "./foo", &g_options));
	cl_assert_equal_s("unsupported URL protocol", git_error_last()->message);
	cl_assert(!git_fs_path_exists("./foo"));
}

void test_clone_nonetwork__do_not_clean_existing_directory(void)
{
	/* Clone should not remove the directory if it already exists, but
	 * Should clean up entries it creates. */
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(git_fs_path_is_empty_dir("./foo"));

	/* Try again with a bare repository. */
	g_options.bare = true;
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(git_fs_path_is_empty_dir("./foo"));
}

void test_clone_nonetwork__local(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__local_absolute_path(void)
{
	const char *local_src;
	local_src = cl_fixture("testrepo.git");
	cl_git_pass(git_clone(&g_repo, local_src, "./foo", &g_options));
}

void test_clone_nonetwork__local_bare(void)
{
	g_options.bare = true;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_when_the_target_is_a_file(void)
{
	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_with_already_existing_but_non_empty_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

static int custom_origin_name_remote_create(
	git_remote **out,
	git_repository *repo,
	const char *name,
	const char *url,
	void *payload)
{
	GIT_UNUSED(name);
	GIT_UNUSED(payload);

	return git_remote_create(out, repo, "my_origin", url);
}

void test_clone_nonetwork__custom_origin_name(void)
{
	g_options.remote_cb = custom_origin_name_remote_create;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_remote_lookup(&g_remote, g_repo, "my_origin"));
}

void test_clone_nonetwork__defaults(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", NULL));
	cl_assert(g_repo);
	cl_git_pass(git_remote_lookup(&g_remote, g_repo, "origin"));
}

void test_clone_nonetwork__cope_with_already_existing_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__can_prevent_the_checkout_of_a_standard_repo(void)
{
	git_str path = GIT_STR_INIT;

	g_options.checkout_opts.checkout_strategy = 0;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_str_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(false, git_fs_path_isfile(git_str_cstr(&path)));

	git_str_dispose(&path);
}

void test_clone_nonetwork__can_checkout_given_branch(void)
{
	git_reference *remote_head;

	g_options.checkout_branch = "test";
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_assert_equal_i(0, git_repository_head_unborn(g_repo));

	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_assert_equal_s(git_reference_name(g_ref), "refs/heads/test");

	cl_assert(git_fs_path_exists("foo/readme.txt"));

	cl_git_pass(git_reference_lookup(&remote_head, g_repo, "refs/remotes/origin/HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(remote_head));
	cl_assert_equal_s("refs/remotes/origin/master", git_reference_symbolic_target(remote_head));

	git_reference_free(remote_head);
}

static void assert_checkout_tag(const char *name, int bare, git_clone_local_t local)
{
	git_object *target, *commit;
	git_reference *remote_head;
	git_config *config;
	git_str config_key = GIT_STR_INIT;
	const char *value, *tag_name = name;

	if (!git__prefixcmp(tag_name, GIT_REFS_TAGS_DIR))
		tag_name += strlen(GIT_REFS_TAGS_DIR);
	g_options.checkout_branch = name;
	g_options.bare = bare;
	g_options.local = local;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_assert_equal_i(1, git_repository_head_detached(g_repo));
	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_git_pass(git_revparse_single(&target, g_repo, name));
	cl_git_pass(git_object_peel(&commit, target, GIT_OBJECT_COMMIT));
	cl_assert_equal_oid(git_object_id(commit), git_reference_target(g_ref));
	git_object_free(commit);
	git_object_free(target);

	cl_git_pass(git_repository_config_snapshot(&config, g_repo));
	cl_git_pass(git_str_printf(&config_key, "branch.%s.remote", tag_name));
	cl_git_fail_with(GIT_ENOTFOUND, git_config_get_string(&value, config, config_key.ptr));
	git_str_dispose(&config_key);
	git_config_free(config);
	cl_git_pass(git_reference_lookup(&remote_head, g_repo, "refs/remotes/origin/HEAD"));
	cl_assert_equal_s("refs/remotes/origin/master", git_reference_symbolic_target(remote_head));
	git_reference_free(remote_head);
	if (!bare)
		cl_assert(git_fs_path_exists(!strcmp(name, "refs/tags/test") ?
			"foo/readme.txt" : "foo/README"));
}

void test_clone_nonetwork__checkout_annotated_tag(void)
{
	assert_checkout_tag("hard_tag", 0, GIT_CLONE_LOCAL_AUTO);
}

static void checkout_created_tag(int annotated)
{
	git_repository *source;
	git_object *target;
	git_signature *signature;
	git_oid id;

	source = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_revparse_single(&target, source, "hard_tag"));
	if (annotated) {
		cl_git_pass(git_signature_now(&signature, "tagger", "tagger@example.com"));
		cl_git_pass(git_tag_create(&id, source, "nested", target, signature, "nested tag", 0));
		git_signature_free(signature);
	} else {
		git_object *commit;
		cl_git_pass(git_object_peel(&commit, target, GIT_OBJECT_COMMIT));
		cl_git_pass(git_tag_create_lightweight(&id, source, "nested", commit, 0));
		git_object_free(commit);
	}
	git_object_free(target);

	g_options.checkout_branch = "nested";
	cl_git_pass(git_clone(&g_repo, cl_git_path_url(git_repository_path(source)), "./foo", &g_options));
	cl_assert_equal_i(1, git_repository_head_detached(g_repo));
	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_git_pass(git_revparse_single(&target, source, "hard_tag^{}"));
	cl_assert_equal_oid(git_object_id(target), git_reference_target(g_ref));
	cl_assert(git_fs_path_exists("foo/README"));
	git_object_free(target);
	cl_git_sandbox_cleanup();
}

void test_clone_nonetwork__checkout_nested_tag(void)
{
	checkout_created_tag(1);
}

void test_clone_nonetwork__checkout_lightweight_tag(void)
{
	checkout_created_tag(0);
}

void test_clone_nonetwork__checkout_tag_full_reference(void)
{
	assert_checkout_tag("refs/tags/test", 0, GIT_CLONE_LOCAL_AUTO);
}

void test_clone_nonetwork__checkout_tag_bare(void)
{
	assert_checkout_tag("hard_tag", 1, GIT_CLONE_LOCAL_AUTO);
}

void test_clone_nonetwork__checkout_tag_without_local_optimization(void)
{
	assert_checkout_tag("hard_tag", 0, GIT_CLONE_NO_LOCAL);
}

void test_clone_nonetwork__checkout_branch_full_reference(void)
{
	g_options.checkout_branch = "refs/heads/test";
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_assert_equal_s("refs/heads/test", git_reference_name(g_ref));
}

void test_clone_nonetwork__checkout_branch_precedes_tag(void)
{
	g_options.checkout_branch = "test";
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_git_pass(git_repository_head(&g_ref, g_repo));
	cl_assert_equal_s("refs/heads/test", git_reference_name(g_ref));
}

void test_clone_nonetwork__checkout_missing_branch_or_tag(void)
{
	g_options.checkout_branch = "does-not-exist";
	cl_git_fail_with(GIT_ENOTFOUND, git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_assert_equal_p(NULL, g_repo);
	cl_assert(!git_fs_path_exists("foo"));
}

void test_clone_nonetwork__checkout_missing_explicit_branch_does_not_use_tag(void)
{
	g_options.checkout_branch = "refs/heads/hard_tag";
	cl_git_fail_with(GIT_ENOTFOUND, git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_assert_equal_p(NULL, g_repo);
	cl_assert(!git_fs_path_exists("foo"));
}

void test_clone_nonetwork__checkout_missing_explicit_tag(void)
{
	g_options.checkout_branch = "refs/tags/does-not-exist";
	cl_git_fail_with(GIT_ENOTFOUND, git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_assert_equal_p(NULL, g_repo);
	cl_assert(!git_fs_path_exists("foo"));
}

void test_clone_nonetwork__checkout_noncommit_tag(void)
{
	g_options.checkout_branch = "point_to_blob";
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	cl_assert_equal_p(NULL, g_repo);
	cl_assert(!git_fs_path_exists("foo"));
}

static int clone_cancel_fetch_transfer_progress_cb(
	const git_indexer_progress *stats, void *data)
{
	GIT_UNUSED(stats); GIT_UNUSED(data);
	return -54321;
}

void test_clone_nonetwork__can_cancel_clone_in_fetch(void)
{
	g_options.checkout_branch = "test";

	g_options.fetch_opts.callbacks.transfer_progress =
		clone_cancel_fetch_transfer_progress_cb;

	cl_git_fail_with(git_clone(
		&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options),
		-54321);

	cl_assert(!g_repo);
	cl_assert(!git_fs_path_exists("foo/readme.txt"));
}

static int clone_cancel_checkout_cb(
	git_checkout_notify_t why,
	const char *path,
	const git_diff_file *b,
	const git_diff_file *t,
	const git_diff_file *w,
	void *payload)
{
	const char *at_file = payload;
	GIT_UNUSED(why); GIT_UNUSED(b); GIT_UNUSED(t); GIT_UNUSED(w);
	if (!strcmp(path, at_file))
		return -12345;
	return 0;
}

void test_clone_nonetwork__can_cancel_clone_in_checkout(void)
{
	g_options.checkout_branch = "test";

	g_options.checkout_opts.notify_flags = GIT_CHECKOUT_NOTIFY_UPDATED;
	g_options.checkout_opts.notify_cb = clone_cancel_checkout_cb;
	g_options.checkout_opts.notify_payload = "readme.txt";

	cl_git_fail_with(git_clone(
		&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options),
		-12345);

	cl_assert(!g_repo);
	cl_assert(!git_fs_path_exists("foo/readme.txt"));
}

void test_clone_nonetwork__can_detached_head(void)
{
	git_object *obj;
	git_repository *cloned;
	git_reference *cloned_head;

	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_revparse_single(&obj, g_repo, "master~1"));
	cl_git_pass(git_repository_set_head_detached(g_repo, git_object_id(obj)));

	cl_git_pass(git_clone(&cloned, "./foo", "./foo1", &g_options));

	cl_assert(git_repository_head_detached(cloned));

	cl_git_pass(git_repository_head(&cloned_head, cloned));
	cl_assert_equal_oid(git_object_id(obj), git_reference_target(cloned_head));

	git_object_free(obj);
	git_reference_free(cloned_head);
	git_repository_free(cloned);

	cl_fixture_cleanup("./foo1");
}

void test_clone_nonetwork__clone_tag_to_tree(void)
{
	git_repository *stage;
	git_index_entry entry;
	git_index *index;
	git_odb *odb;
	git_oid tree_id;
	git_tree *tree;
	git_reference *tag;
	git_tree_entry *tentry;
	git_index_options index_opts = GIT_INDEX_OPTIONS_INIT;

	const char *file_path = "some/deep/path.txt";
	const char *file_content = "some content\n";
	const char *tag_name = "refs/tags/tree-tag";

	index_opts.oid_type = GIT_OID_SHA1;

	stage = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_odb(&odb, stage));
	cl_git_pass(git_index_new_ext(&index, &index_opts));

	memset(&entry, 0, sizeof(git_index_entry));
	entry.path = file_path;
	entry.mode = GIT_FILEMODE_BLOB;
	cl_git_pass(git_odb_write(&entry.id, odb, file_content, strlen(file_content), GIT_OBJECT_BLOB));

	cl_git_pass(git_index_add(index, &entry));
	cl_git_pass(git_index_write_tree_to(&tree_id, index, stage));
	cl_git_pass(git_reference_create(&tag, stage, tag_name, &tree_id, 0, NULL));
	git_reference_free(tag);
	git_odb_free(odb);
	git_index_free(index);

	g_options.local = GIT_CLONE_NO_LOCAL;
	cl_git_pass(git_clone(&g_repo, cl_git_path_url(git_repository_path(stage)), "./foo", &g_options));
	git_repository_free(stage);

	cl_git_pass(git_reference_lookup(&tag, g_repo, tag_name));
	cl_git_pass(git_tree_lookup(&tree, g_repo, git_reference_target(tag)));
	git_reference_free(tag);

	cl_git_pass(git_tree_entry_bypath(&tentry, tree, file_path));
	git_tree_entry_free(tentry);
	git_tree_free(tree);

	cl_fixture_cleanup("testrepo.git");
}

static void assert_correct_reflog(const char *name)
{
	git_reflog *log;
	const git_reflog_entry *entry;
	git_str expected_message = GIT_STR_INIT;

	git_str_printf(&expected_message,
		"clone: from %s", cl_git_fixture_url("testrepo.git"));

	cl_git_pass(git_reflog_read(&log, g_repo, name));
	cl_assert_equal_i(1, git_reflog_entrycount(log));
	entry = git_reflog_entry_byindex(log, 0);
	cl_assert_equal_s(expected_message.ptr, git_reflog_entry_message(entry));

	git_reflog_free(log);

	git_str_dispose(&expected_message);
}

void test_clone_nonetwork__clone_updates_reflog_properly(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
	assert_correct_reflog("HEAD");
	assert_correct_reflog("refs/heads/master");
}

static void cleanup_repository(void *path)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup((const char *)path);
}

void test_clone_nonetwork__clone_from_empty_sets_upstream(void)
{
	git_config *config;
	git_repository *repo;
	const char *str;

	/* Create an empty repo to clone from */
	cl_set_cleanup(&cleanup_repository, "./test1");
	cl_git_pass(git_repository_init(&g_repo, "./test1", 0));
	cl_set_cleanup(&cleanup_repository, "./repowithunborn");
	cl_git_pass(git_clone(&repo, "./test1", "./repowithunborn", NULL));

	cl_git_pass(git_repository_config_snapshot(&config, repo));

	cl_git_pass(git_config_get_string(&str, config, "branch.master.remote"));
	cl_assert_equal_s("origin", str);
	cl_git_pass(git_config_get_string(&str, config, "branch.master.merge"));
	cl_assert_equal_s("refs/heads/master", str);

	git_config_free(config);
	git_repository_free(repo);
	cl_fixture_cleanup("./repowithunborn");
}
