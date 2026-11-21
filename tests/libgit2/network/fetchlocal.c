#include "clar_libgit2.h"

#include "path.h"
#include "remote.h"
#include "futils.h"

static const char* tagger_name = "Vicent Marti";
static const char* tagger_email = "vicent@github.com";
static const char* tagger_message = "This is my tag.\n\nThere are many tags, but this one is mine\n";

static int transfer_cb(const git_indexer_progress *stats, void *payload)
{
	int *callcount = (int*)payload;
	GIT_UNUSED(stats);
	(*callcount)++;
	return 0;
}

static void cleanup_local_repo(void *path)
{
	cl_fixture_cleanup((char *)path);
}

void test_network_fetchlocal__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_network_fetchlocal__complete(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};

	const char *url = cl_git_fixture_url("testrepo.git");
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(20, (int)refnames.count);
	cl_assert(callcount > 0);

	git_strarray_dispose(&refnames);
	git_remote_free(origin);
	git_repository_free(repo);
}

void test_network_fetchlocal__prune(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};
	git_reference *ref;
	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(20, (int)refnames.count);
	cl_assert(callcount > 0);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	cl_git_pass(git_reference_lookup(&ref, remote_repo, "refs/heads/br2"));
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);

	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));
	cl_git_pass(git_remote_prune(origin, &options.callbacks));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(19, (int)refnames.count);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	cl_git_pass(git_reference_lookup(&ref, remote_repo, "refs/heads/packed"));
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);

	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));
	cl_git_pass(git_remote_prune(origin, &options.callbacks));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(18, (int)refnames.count);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	git_repository_free(repo);
}

static int update_refs_fail_on_call(const char *ref, const git_oid *old, const git_oid *new, git_refspec *refspec, void *data)
{
	GIT_UNUSED(ref);
	GIT_UNUSED(old);
	GIT_UNUSED(new);
	GIT_UNUSED(refspec);
	GIT_UNUSED(data);

	cl_fail("update refs called");
	return 0;
}

static void assert_ref_exists(git_repository *repo, const char *name)
{
	git_reference *ref;

	cl_git_pass(git_reference_lookup(&ref, repo, name));
	git_reference_free(ref);
}

void test_network_fetchlocal__prune_overlapping(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};
	git_reference *ref;
	git_config *config;
	git_oid target;

	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));

	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_git_pass(git_reference_lookup(&ref, remote_repo, "refs/heads/master"));
	git_oid_cpy(&target, git_reference_target(ref));
	git_reference_free(ref);
	cl_git_pass(git_reference_create(&ref, remote_repo, "refs/pull/42/head", &target, 1, NULL));
	git_reference_free(ref);

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_bool(config, "remote.origin.prune", true));
	cl_git_pass(git_config_set_multivar(config, "remote.origin.fetch", "^$", "refs/pull/*/head:refs/remotes/origin/pr/*"));

	git_remote_free(origin);
	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	assert_ref_exists(repo, "refs/remotes/origin/master");
	assert_ref_exists(repo, "refs/remotes/origin/pr/42");
	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(21, (int)refnames.count);
	git_strarray_dispose(&refnames);

	cl_git_pass(git_config_delete_multivar(config, "remote.origin.fetch", "refs"));
	cl_git_pass(git_config_set_multivar(config, "remote.origin.fetch", "^$", "refs/pull/*/head:refs/remotes/origin/pr/*"));
	cl_git_pass(git_config_set_multivar(config, "remote.origin.fetch", "^$", "refs/heads/*:refs/remotes/origin/*"));

	git_remote_free(origin);
	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	options.callbacks.update_refs = update_refs_fail_on_call;
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	assert_ref_exists(repo, "refs/remotes/origin/master");
	assert_ref_exists(repo, "refs/remotes/origin/pr/42");
	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(21, (int)refnames.count);
	git_strarray_dispose(&refnames);

	cl_git_pass(git_config_delete_multivar(config, "remote.origin.fetch", "refs"));
	cl_git_pass(git_config_set_multivar(config, "remote.origin.fetch", "^$", "refs/heads/*:refs/remotes/origin/*"));
	cl_git_pass(git_config_set_multivar(config, "remote.origin.fetch", "^$", "refs/pull/*/head:refs/remotes/origin/pr/*"));

	git_remote_free(origin);
	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	options.callbacks.update_refs = update_refs_fail_on_call;
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	git_config_free(config);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);
	git_repository_free(repo);
}

void test_network_fetchlocal__fetchprune(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};
	git_reference *ref;
	git_config *config;
	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(20, (int)refnames.count);
	cl_assert(callcount > 0);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	cl_git_pass(git_reference_lookup(&ref, remote_repo, "refs/heads/br2"));
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);

	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));
	cl_git_pass(git_remote_prune(origin, &options.callbacks));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(19, (int)refnames.count);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	cl_git_pass(git_reference_lookup(&ref, remote_repo, "refs/heads/packed"));
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_bool(config, "remote.origin.prune", 1));
	git_config_free(config);
	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_assert_equal_i(1, git_remote_prune_refs(origin));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(18, (int)refnames.count);
	git_strarray_dispose(&refnames);
	git_remote_free(origin);

	git_repository_free(repo);
}

void test_network_fetchlocal__prune_tag(void)
{
	git_repository *repo;
	git_remote *origin;
	int callcount = 0;
	git_reference *ref;
	git_config *config;
	git_oid tag_id;
	git_signature *tagger;
	git_object *obj;

	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));
	git_remote_free(origin);

	cl_git_pass(git_revparse_single(&obj, repo, "origin/master"));

	cl_git_pass(git_reference_create(&ref, repo, "refs/remotes/origin/fake-remote", git_object_id(obj), 1, NULL));
	git_reference_free(ref);

	/* create signature */
	cl_git_pass(git_signature_new(&tagger, tagger_name, tagger_email, 123456789, 60));

	cl_git_pass(
		git_tag_create(&tag_id, repo,
		  "some-tag", obj, tagger, tagger_message, 0)
	);
	git_signature_free(tagger);

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_bool(config, "remote.origin.prune", 1));
	git_config_free(config);
	cl_git_pass(git_remote_lookup(&origin, repo, GIT_REMOTE_ORIGIN));
	cl_assert_equal_i(1, git_remote_prune_refs(origin));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	assert_ref_exists(repo, "refs/tags/some-tag");
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_lookup(&ref, repo, "refs/remotes/origin/fake-remote"));

	git_object_free(obj);
	git_remote_free(origin);

	git_repository_free(repo);
}

void test_network_fetchlocal__partial(void)
{
	git_repository *repo = cl_git_sandbox_init("partial-testrepo");
	git_remote *origin;
	int callcount = 0;
	git_strarray refnames = {0};
	const char *url;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	options.callbacks.payload = &callcount;

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(1, (int)refnames.count);

	url = cl_git_fixture_url("testrepo.git");
	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_git_pass(git_remote_fetch(origin, NULL, &options, NULL));

	git_strarray_dispose(&refnames);

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(21, (int)refnames.count); /* 18 remote + 1 local */
	cl_assert(callcount > 0);

	git_strarray_dispose(&refnames);
	git_remote_free(origin);
}

static int remote_mirror_cb(git_remote **out, git_repository *repo,
			    const char *name, const char *url, void *payload)
{
	int error;
	git_remote *remote;

	GIT_UNUSED(payload);

	if ((error = git_remote_create_with_fetchspec(&remote, repo, name, url, "+refs/*:refs/*")) < 0)
		return error;

	*out = remote;
	return 0;
}

void test_network_fetchlocal__clone_into_mirror(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *repo;
	git_reference *ref;

	opts.bare = true;
	opts.remote_cb = remote_mirror_cb;
	cl_git_pass(git_clone(&repo, cl_git_fixture_url("testrepo.git"), "./foo.git", &opts));

	cl_git_pass(git_reference_lookup(&ref, repo, "HEAD"));
	cl_assert_equal_i(GIT_REFERENCE_SYMBOLIC, git_reference_type(ref));
	cl_assert_equal_s("refs/heads/master", git_reference_symbolic_target(ref));

	git_reference_free(ref);
	cl_git_pass(git_reference_lookup(&ref, repo, "refs/remotes/test/master"));

	git_reference_free(ref);
	git_repository_free(repo);
	cl_fixture_cleanup("./foo.git");
}

void test_network_fetchlocal__all_refs(void)
{
	git_repository *repo;
	git_remote *remote;
	git_reference *ref;
	char *allrefs = "+refs/*:refs/*";
	git_strarray refspecs = {
		&allrefs,
		1,
	};

	cl_git_pass(git_repository_init(&repo, "./foo.git", true));
	cl_git_pass(git_remote_create_anonymous(&remote, repo, cl_git_fixture_url("testrepo.git")));
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));

	cl_git_pass(git_reference_lookup(&ref, repo, "refs/remotes/test/master"));
	git_reference_free(ref);

	cl_git_pass(git_reference_lookup(&ref, repo, "refs/tags/test"));
	git_reference_free(ref);

	git_remote_free(remote);
	git_repository_free(repo);
	cl_fixture_cleanup("./foo.git");
}

void test_network_fetchlocal__multi_remotes(void)
{
	git_repository *repo = cl_git_sandbox_init("testrepo.git");
	git_remote *test, *test2;
	git_strarray refnames = {0};
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;

	options.callbacks.transfer_progress = transfer_cb;
	cl_git_pass(git_remote_set_url(repo, "test", cl_git_fixture_url("testrepo.git")));
	cl_git_pass(git_remote_lookup(&test, repo, "test"));
	cl_git_pass(git_remote_fetch(test, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(35, (int)refnames.count);
	git_strarray_dispose(&refnames);

	cl_git_pass(git_remote_set_url(repo, "test_with_pushurl", cl_git_fixture_url("testrepo.git")));
	cl_git_pass(git_remote_lookup(&test2, repo, "test_with_pushurl"));
	cl_git_pass(git_remote_fetch(test2, NULL, &options, NULL));

	cl_git_pass(git_reference_list(&refnames, repo));
	cl_assert_equal_i(48, (int)refnames.count);

	git_strarray_dispose(&refnames);
	git_remote_free(test);
	git_remote_free(test2);
}

static int sideband_cb(const char *str, int len, void *payload)
{
	int *count = (int *) payload;

	GIT_UNUSED(str);
	GIT_UNUSED(len);

	(*count)++;
	return 0;
}

void test_network_fetchlocal__call_progress(void)
{
	git_repository *repo;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	int callcount = 0;

	cl_git_pass(git_repository_init(&repo, "foo.git", true));
	cl_set_cleanup(cleanup_local_repo, "foo.git");

	cl_git_pass(git_remote_create_with_fetchspec(&remote, repo, "origin", cl_git_fixture_url("testrepo.git"), "+refs/heads/*:refs/heads/*"));

	options.callbacks.sideband_progress = sideband_cb;
	options.callbacks.payload = &callcount;

	cl_git_pass(git_remote_fetch(remote, NULL, &options, NULL));
	cl_assert(callcount != 0);

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__prune_load_remote_prune_config(void)
{
	git_repository *repo;
	git_remote *origin;
	git_config *config;
	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_bool(config, "remote.origin.prune", 1));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_assert_equal_i(1, git_remote_prune_refs(origin));

	git_config_free(config);
	git_remote_free(origin);
	git_repository_free(repo);
}

void test_network_fetchlocal__prune_load_fetch_prune_config(void)
{
	git_repository *repo;
	git_remote *origin;
	git_config *config;
	git_repository *remote_repo = cl_git_sandbox_init("testrepo.git");
	const char *url = cl_git_path_url(git_repository_path(remote_repo));

	cl_set_cleanup(&cleanup_local_repo, "foo");
	cl_git_pass(git_repository_init(&repo, "foo", true));

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_bool(config, "fetch.prune", 1));

	cl_git_pass(git_remote_create(&origin, repo, GIT_REMOTE_ORIGIN, url));
	cl_assert_equal_i(1, git_remote_prune_refs(origin));

	git_config_free(config);
	git_remote_free(origin);
	git_repository_free(repo);
}

static int update_refs_error(const char *ref, const git_oid *old, const git_oid *new, git_refspec *refspec, void *data)
{
	int *callcount = (int *) data;

	GIT_UNUSED(ref);
	GIT_UNUSED(old);
	GIT_UNUSED(new);
	GIT_UNUSED(refspec);

	(*callcount)++;

	return -1;
}

void test_network_fetchlocal__update_refs_error_is_propagated(void)
{
	git_repository *repo;
	git_reference_iterator *iterator;
	git_reference *ref;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	int callcount = 0;

	cl_git_pass(git_repository_init(&repo, "foo.git", true));
	cl_set_cleanup(cleanup_local_repo, "foo.git");

	cl_git_pass(git_remote_create_with_fetchspec(&remote, repo, "origin", cl_git_fixture_url("testrepo.git"), "+refs/heads/*:refs/remotes/update-tips/*"));

	options.callbacks.update_refs = update_refs_error;
	options.callbacks.payload = &callcount;

	cl_git_fail(git_remote_fetch(remote, NULL, &options, NULL));
	cl_assert_equal_i(1, callcount);

	cl_git_pass(git_reference_iterator_glob_new(&iterator, repo, "refs/remotes/update-tips/**/"));
	cl_assert_equal_i(GIT_ITEROVER, git_reference_next(&ref, iterator));

	git_reference_iterator_free(iterator);
	git_remote_free(remote);
	git_repository_free(repo);
}

#ifndef GIT_DEPRECATE_HARD
static int update_tips(const char *ref, const git_oid *old, const git_oid *new, void *data)
{
	int *called = (int *) data;

	GIT_UNUSED(ref);
	GIT_UNUSED(old);
	GIT_UNUSED(new);

	(*called) += 1;

	return 0;
}

static int update_refs(const char *ref, const git_oid *old, const git_oid *new, git_refspec *spec, void *data)
{
	int *called = (int *) data;

	GIT_UNUSED(ref);
	GIT_UNUSED(old);
	GIT_UNUSED(new);
	GIT_UNUSED(spec);

	(*called) += 0x10000;

	return 0;
}
#endif

void test_network_fetchlocal__update_tips_backcompat(void)
{
#ifndef GIT_DEPRECATE_HARD
	git_repository *repo;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	int callcount = 0;

	cl_git_pass(git_repository_init(&repo, "foo.git", true));
	cl_set_cleanup(cleanup_local_repo, "foo.git");

	cl_git_pass(git_remote_create_with_fetchspec(&remote, repo, "origin", cl_git_fixture_url("testrepo.git"), "+refs/heads/*:refs/remotes/update-tips/*"));

	options.callbacks.update_tips = update_tips;
	options.callbacks.payload = &callcount;

	cl_git_pass(git_remote_fetch(remote, NULL, &options, NULL));
	cl_assert_equal_i(0, (callcount & 0xffff0000));
	cl_assert((callcount & 0x0000ffff) > 0);

	git_remote_free(remote);
	git_repository_free(repo);
#endif
}

void test_network_fetchlocal__update_refs_is_preferred(void)
{
#ifndef GIT_DEPRECATE_HARD
	git_repository *repo;
	git_remote *remote;
	git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
	int callcount = 0;

	cl_git_pass(git_repository_init(&repo, "foo.git", true));
	cl_set_cleanup(cleanup_local_repo, "foo.git");

	cl_git_pass(git_remote_create_with_fetchspec(&remote, repo, "origin", cl_git_fixture_url("testrepo.git"), "+refs/heads/*:refs/remotes/update-tips/*"));

	options.callbacks.update_tips = update_tips;
	options.callbacks.update_refs = update_refs;
	options.callbacks.payload = &callcount;

	cl_git_pass(git_remote_fetch(remote, NULL, &options, NULL));
	cl_assert_equal_i(0, (callcount & 0x0000ffff));
	cl_assert((callcount & 0xffff0000) > 0);

	git_remote_free(remote);
	git_repository_free(repo);
#endif
}

static git_repository *setup_negative_fetch(
	git_repository **repo, git_remote **remote)
{
	git_repository *source = cl_git_sandbox_init("testrepo.git");
	git_reference *ref;
	git_oid id;

	cl_git_pass(git_reference_name_to_id(&id, source, "refs/heads/master"));
	cl_git_pass(git_reference_create(&ref, source, "refs/heads/negative-private",
		&id, false, NULL));
	git_reference_free(ref);
	cl_git_pass(git_reference_create(&ref, source, "refs/heads/negative-gone",
		&id, false, NULL));
	git_reference_free(ref);

	cl_set_cleanup(&cleanup_local_repo, "negative-fetch");
	cl_git_pass(git_repository_init(repo, "negative-fetch", true));
	cl_git_pass(git_remote_create(remote, *repo, "origin",
		cl_git_path_url(git_repository_path(source))));
	return source;
}

static void fetch_excluding(
	git_repository **repo, git_remote **remote, char *negative)
{
	char *strings[] = {
		"+refs/heads/*:refs/remotes/origin/*",
		negative,
	};
	git_strarray refspecs = { strings, ARRAY_SIZE(strings) };

	setup_negative_fetch(repo, remote);
	cl_git_pass(git_remote_fetch(*remote, &refspecs, NULL, NULL));
}

static void configure_negative_fetch(
	git_repository *repo, git_remote **remote, const char *negative)
{
	cl_git_pass(git_remote_add_fetch(repo, "origin", negative));
	git_remote_free(*remote);
	cl_git_pass(git_remote_lookup(remote, repo, "origin"));
}

void test_network_fetchlocal__negative_refspec_does_not_create_reference(void)
{
	git_repository *repo;
	git_remote *remote;
	git_oid id;

	fetch_excluding(&repo, &remote, "^refs/heads/negative-private");
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-private"));
	assert_ref_exists(repo, "refs/remotes/origin/negative-gone");

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_refspec_omits_fetchhead(void)
{
	git_repository *repo;
	git_remote *remote;
	git_str path = GIT_STR_INIT, contents = GIT_STR_INIT;

	fetch_excluding(&repo, &remote, "^refs/heads/negative-private");
	cl_git_pass(git_str_joinpath(&path, git_repository_path(repo), "FETCH_HEAD"));
	cl_git_pass(git_futils_readbuffer(&contents, path.ptr));
	cl_assert(strstr(contents.ptr, "negative-private") == NULL);
	cl_assert(strstr(contents.ptr, "negative-gone") != NULL);

	git_str_dispose(&contents);
	git_str_dispose(&path);
	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_wildcard_excludes_references(void)
{
	git_repository *repo;
	git_remote *remote;
	git_oid id;

	fetch_excluding(&repo, &remote, "^refs/heads/negative-*");
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-private"));
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-gone"));
	assert_ref_exists(repo, "refs/remotes/origin/master");

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_refspec_preserves_existing_reference(void)
{
	git_repository *repo, *source;
	git_remote *remote;
	git_reference *ref;
	git_oid old, replacement, actual;

	source = setup_negative_fetch(&repo, &remote);
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	cl_git_pass(git_reference_name_to_id(&old, repo,
		"refs/remotes/origin/negative-private"));
	cl_git_pass(git_reference_name_to_id(&replacement, source, "refs/heads/haacked"));
	cl_assert(!git_oid_equal(&old, &replacement));
	cl_git_pass(git_reference_create(&ref, source, "refs/heads/negative-private",
		&replacement, true, NULL));
	git_reference_free(ref);

	configure_negative_fetch(repo, &remote, "^refs/heads/negative-private");
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	cl_git_pass(git_reference_name_to_id(&actual, repo,
		"refs/remotes/origin/negative-private"));
	cl_assert_equal_oid(&old, &actual);

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_refspec_preserves_pruned_reference(void)
{
	git_repository *repo, *source;
	git_remote *remote;
	git_fetch_options opts = GIT_FETCH_OPTIONS_INIT;
	git_oid id;

	source = setup_negative_fetch(&repo, &remote);
	cl_git_pass(git_remote_fetch(remote, NULL, NULL, NULL));
	cl_git_pass(git_reference_remove(source, "refs/heads/negative-private"));
	cl_git_pass(git_reference_remove(source, "refs/heads/negative-gone"));
	configure_negative_fetch(repo, &remote, "^refs/heads/negative-private");

	opts.prune = GIT_FETCH_PRUNE;
	cl_git_pass(git_remote_fetch(remote, NULL, &opts, NULL));
	assert_ref_exists(repo, "refs/remotes/origin/negative-private");
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-gone"));

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_passive_refspec_blocks_opportunistic_update(void)
{
	git_repository *repo;
	git_remote *remote;
	git_oid id;
	char *spec = "refs/heads/negative-private";
	git_strarray refspecs = { &spec, 1 };

	setup_negative_fetch(&repo, &remote);
	configure_negative_fetch(repo, &remote, "^refs/heads/negative-private");
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-private"));

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_active_refspec_blocks_opportunistic_update(void)
{
	git_repository *repo;
	git_remote *remote;
	git_oid id;
	char *strings[] = { "refs/heads/negative-private",
		"refs/heads/negative-gone", "^refs/heads/negative-private" };
	git_strarray refspecs = { strings, ARRAY_SIZE(strings) };

	setup_negative_fetch(&repo, &remote);
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));
	cl_git_fail_with(GIT_ENOTFOUND, git_reference_name_to_id(&id, repo,
		"refs/remotes/origin/negative-private"));
	assert_ref_exists(repo, "refs/remotes/origin/negative-gone");

	git_remote_free(remote);
	git_repository_free(repo);
}

void test_network_fetchlocal__negative_refspec_preserves_tag_autofollow(void)
{
	git_repository *repo;
	git_remote *remote;

	fetch_excluding(&repo, &remote, "^refs/tags/hard_tag");
	assert_ref_exists(repo, "refs/tags/hard_tag");

	git_remote_free(remote);
	git_repository_free(repo);
}
