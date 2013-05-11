/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/config.h"
#include "git2/types.h"
#include "git2/oid.h"
#include "git2/net.h"

#include "config.h"
#include "repository.h"
#include "remote.h"
#include "fetch.h"
#include "refs.h"
#include "refspec.h"
#include "fetchhead.h"

#include <regex.h>

static int add_refspec(git_remote *remote, const char *string, bool is_fetch)
{
	git_refspec *spec;

	spec = git__calloc(1, sizeof(git_refspec));
	GITERR_CHECK_ALLOC(spec);

	if (git_refspec__parse(spec, string, is_fetch) < 0) {
		git__free(spec);
		return -1;
	}

	spec->push = !is_fetch;
	if (git_vector_insert(&remote->refspecs, spec) < 0) {
		git_refspec__free(spec);
		git__free(spec);
		return -1;
	}

	return 0;
}

static int download_tags_value(git_remote *remote, git_config *cfg)
{
	const char *val;
	git_buf buf = GIT_BUF_INIT;
	int error;

	if (remote->download_tags != GIT_REMOTE_DOWNLOAD_TAGS_UNSET)
		return 0;

	/* This is the default, let's see if we need to change it */
	remote->download_tags = GIT_REMOTE_DOWNLOAD_TAGS_AUTO;
	if (git_buf_printf(&buf, "remote.%s.tagopt", remote->name) < 0)
		return -1;

	error = git_config_get_string(&val, cfg, git_buf_cstr(&buf));
	git_buf_free(&buf);
	if (!error && !strcmp(val, "--no-tags"))
		remote->download_tags = GIT_REMOTE_DOWNLOAD_TAGS_NONE;
	else if (!error && !strcmp(val, "--tags"))
		remote->download_tags = GIT_REMOTE_DOWNLOAD_TAGS_ALL;

	if (error == GIT_ENOTFOUND) {
		giterr_clear();
		error = 0;
	}

	return error;
}

static int ensure_remote_name_is_valid(const char *name)
{
	int error = 0;

	if (!git_remote_is_valid_name(name)) {
		giterr_set(
			GITERR_CONFIG,
			"'%s' is not a valid remote name.", name);
		error = GIT_EINVALIDSPEC;
	}

	return error;
}

static int create_internal(git_remote **out, git_repository *repo, const char *name, const char *url, const char *fetch)
{
	git_remote *remote;
	git_buf fetchbuf = GIT_BUF_INIT;
	int error = -1;

	/* name is optional */
	assert(out && repo && url);

	remote = git__calloc(1, sizeof(git_remote));
	GITERR_CHECK_ALLOC(remote);

	remote->repo = repo;
	remote->check_cert = 1;
	remote->update_fetchhead = 1;

	if (git_vector_init(&remote->refs, 32, NULL) < 0)
		goto on_error;

	remote->url = git__strdup(url);
	GITERR_CHECK_ALLOC(remote->url);

	if (name != NULL) {
		remote->name = git__strdup(name);
		GITERR_CHECK_ALLOC(remote->name);
	}

	if (fetch != NULL) {
		if (add_refspec(remote, fetch, true) < 0)
			goto on_error;
	}

	/* A remote without a name doesn't download tags */
	if (!name) {
		remote->download_tags = GIT_REMOTE_DOWNLOAD_TAGS_NONE;
	}

	*out = remote;
	git_buf_free(&fetchbuf);
	return 0;

on_error:
	git_remote_free(remote);
	git_buf_free(&fetchbuf);
	return error;
}

static int ensure_remote_doesnot_exist(git_repository *repo, const char *name)
{
	int error;
	git_remote *remote;

	error = git_remote_load(&remote, repo, name);

	if (error == GIT_ENOTFOUND)
		return 0;

	if (error < 0)
		return error;

	git_remote_free(remote);

	giterr_set(
		GITERR_CONFIG,
		"Remote '%s' already exists.", name);

	return GIT_EEXISTS;
}


int git_remote_create(git_remote **out, git_repository *repo, const char *name, const char *url)
{
	git_buf buf = GIT_BUF_INIT;
	git_remote *remote = NULL;
	int error;

	if ((error = ensure_remote_name_is_valid(name)) < 0)
		return error;

	if ((error = ensure_remote_doesnot_exist(repo, name)) < 0)
		return error;

	if (git_buf_printf(&buf, "+refs/heads/*:refs/remotes/%s/*", name) < 0)
		return -1;

	if (create_internal(&remote, repo, name, url, git_buf_cstr(&buf)) < 0)
		goto on_error;

	git_buf_free(&buf);

	if (git_remote_save(remote) < 0)
		goto on_error;

	*out = remote;

	return 0;

on_error:
	git_buf_free(&buf);
	git_remote_free(remote);
	return -1;
}

int git_remote_create_inmemory(git_remote **out, git_repository *repo, const char *fetch, const char *url)
{
	int error;
	git_remote *remote;

	if ((error = create_internal(&remote, repo, NULL, url, fetch)) < 0)
		return error;

	*out = remote;
	return 0;
}

struct refspec_cb_data {
	git_remote *remote;
	int fetch;
};

static int refspec_cb(const git_config_entry *entry, void *payload)
{
	const struct refspec_cb_data *data = (struct refspec_cb_data *)payload;

	return add_refspec(data->remote, entry->value, data->fetch);
}

static int get_optional_config(
	git_config *config, git_buf *buf, git_config_foreach_cb cb, void *payload)
{
	int error = 0;
	const char *key = git_buf_cstr(buf);

	if (git_buf_oom(buf))
		return -1;

	if (cb != NULL)
		error = git_config_get_multivar(config, key, NULL, cb, payload);
	else
		error = git_config_get_string(payload, config, key);

	if (error == GIT_ENOTFOUND) {
		giterr_clear();
		error = 0;
	}

	if (error < 0)
		error = -1;

	return error;
}

int git_remote_load(git_remote **out, git_repository *repo, const char *name)
{
	git_remote *remote;
	git_buf buf = GIT_BUF_INIT;
	const char *val;
	int error = 0;
	git_config *config;
	struct refspec_cb_data data;


	assert(out && repo && name);

	if ((error = ensure_remote_name_is_valid(name)) < 0)
		return error;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	remote = git__malloc(sizeof(git_remote));
	GITERR_CHECK_ALLOC(remote);

	memset(remote, 0x0, sizeof(git_remote));
	remote->check_cert = 1;
	remote->update_fetchhead = 1;
	remote->name = git__strdup(name);
	GITERR_CHECK_ALLOC(remote->name);

	if ((git_vector_init(&remote->refs, 32, NULL) < 0) ||
	    (git_vector_init(&remote->refspecs, 2, NULL))) {
		error = -1;
		goto cleanup;
	}

	if (git_buf_printf(&buf, "remote.%s.url", name) < 0) {
		error = -1;
		goto cleanup;
	}

	if ((error = git_config_get_string(&val, config, git_buf_cstr(&buf))) < 0)
		goto cleanup;

	if (strlen(val) == 0) {
		giterr_set(GITERR_INVALID, "Malformed remote '%s' - missing URL", name);
		error = -1;
		goto cleanup;
	}

	remote->repo = repo;
	remote->url = git__strdup(val);
	GITERR_CHECK_ALLOC(remote->url);

	val = NULL;
	git_buf_clear(&buf);
	git_buf_printf(&buf, "remote.%s.pushurl", name);

	if ((error = get_optional_config(config, &buf, NULL, (void *)&val)) < 0)
		goto cleanup;

	if (val) {
		remote->pushurl = git__strdup(val);
		GITERR_CHECK_ALLOC(remote->pushurl);
	}

	data.remote = remote;
	data.fetch = true;
	git_buf_clear(&buf);
	git_buf_printf(&buf, "remote.%s.fetch", name);

	if ((error = get_optional_config(config, &buf, refspec_cb, &data)) < 0)
		goto cleanup;

	data.fetch = false;
	git_buf_clear(&buf);
	git_buf_printf(&buf, "remote.%s.push", name);

	if ((error = get_optional_config(config, &buf, refspec_cb, &data)) < 0)
		goto cleanup;

	if (download_tags_value(remote, config) < 0)
		goto cleanup;

	*out = remote;

cleanup:
	git_buf_free(&buf);

	if (error < 0)
		git_remote_free(remote);

	return error;
}

static int update_config_refspec(const git_remote *remote, git_config *config, int direction)
{
	git_buf name = GIT_BUF_INIT;
	int push;
	const char *dir;
	size_t i;
	int error = 0;

	push = direction == GIT_DIRECTION_PUSH;
	dir = push ? "push" : "fetch";

	if (git_buf_printf(&name, "remote.%s.%s", remote->name, dir) < 0)
		return -1;

	/* Clear out the existing config */
	while (!error)
		error = git_config_delete_entry(config, git_buf_cstr(&name));

	if (error != GIT_ENOTFOUND)
		return error;

	for (i = 0; i < remote->refspecs.length; i++) {
		git_refspec *spec = git_vector_get(&remote->refspecs, i);

		if (spec->push != push)
			continue;

		if ((error = git_config_set_multivar(
				config, git_buf_cstr(&name), "", spec->string)) < 0) {
			goto cleanup;
		}
	}

	giterr_clear();
	error = 0;

cleanup:
	git_buf_free(&name);

	return error;
}

int git_remote_save(const git_remote *remote)
{
	int error;
	git_config *config;
	const char *tagopt = NULL;
	git_buf buf = GIT_BUF_INIT;

	assert(remote);

	if (!remote->name) {
		giterr_set(GITERR_INVALID, "Can't save an in-memory remote.");
		return GIT_EINVALIDSPEC;
	}

	if ((error = ensure_remote_name_is_valid(remote->name)) < 0)
		return error;

	if (git_repository_config__weakptr(&config, remote->repo) < 0)
		return -1;

	if (git_buf_printf(&buf, "remote.%s.url", remote->name) < 0)
		return -1;

	if (git_config_set_string(config, git_buf_cstr(&buf), remote->url) < 0) {
		git_buf_free(&buf);
		return -1;
	}

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.pushurl", remote->name) < 0)
		return -1;

	if (remote->pushurl) {
		if (git_config_set_string(config, git_buf_cstr(&buf), remote->pushurl) < 0) {
			git_buf_free(&buf);
			return -1;
		}
	} else {
		int error = git_config_delete_entry(config, git_buf_cstr(&buf));
		if (error == GIT_ENOTFOUND) {
			error = 0;
			giterr_clear();
		}
		if (error < 0) {
			git_buf_free(&buf);
			return -1;
		}
	}

	if (update_config_refspec(remote, config, GIT_DIRECTION_FETCH) < 0)
		goto on_error;

	if (update_config_refspec(remote, config, GIT_DIRECTION_PUSH) < 0)
		goto on_error;

	/*
	 * What action to take depends on the old and new values. This
	 * is describes by the table below. tagopt means whether the
	 * is already a value set in the config
	 *
	 *            AUTO     ALL or NONE
	 *         +-----------------------+
	 *  tagopt | remove  |     set     |
	 *         +---------+-------------|
	 * !tagopt | nothing |     set     |
	 *         +---------+-------------+
	 */

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.tagopt", remote->name) < 0)
		goto on_error;

	error = git_config_get_string(&tagopt, config, git_buf_cstr(&buf));
	if (error < 0 && error != GIT_ENOTFOUND)
		goto on_error;

	if (remote->download_tags == GIT_REMOTE_DOWNLOAD_TAGS_ALL) {
		if (git_config_set_string(config, git_buf_cstr(&buf), "--tags") < 0)
			goto on_error;
	} else if (remote->download_tags == GIT_REMOTE_DOWNLOAD_TAGS_NONE) {
		if (git_config_set_string(config, git_buf_cstr(&buf), "--no-tags") < 0)
			goto on_error;
	} else if (tagopt) {
		if (git_config_delete_entry(config, git_buf_cstr(&buf)) < 0)
			goto on_error;
	}

	git_buf_free(&buf);

	return 0;

on_error:
	git_buf_free(&buf);
	return -1;
}

const char *git_remote_name(const git_remote *remote)
{
	assert(remote);
	return remote->name;
}

const char *git_remote_url(const git_remote *remote)
{
	assert(remote);
	return remote->url;
}

int git_remote_set_url(git_remote *remote, const char* url)
{
	assert(remote);
	assert(url);

	git__free(remote->url);
	remote->url = git__strdup(url);
	GITERR_CHECK_ALLOC(remote->url);

	return 0;
}

const char *git_remote_pushurl(const git_remote *remote)
{
	assert(remote);
	return remote->pushurl;
}

int git_remote_set_pushurl(git_remote *remote, const char* url)
{
	assert(remote);

	git__free(remote->pushurl);
	if (url) {
		remote->pushurl = git__strdup(url);
		GITERR_CHECK_ALLOC(remote->pushurl);
	} else {
		remote->pushurl = NULL;
	}
	return 0;
}

const char* git_remote__urlfordirection(git_remote *remote, int direction)
{
	assert(remote);

	if (direction == GIT_DIRECTION_FETCH) {
		return remote->url;
	}

	if (direction == GIT_DIRECTION_PUSH) {
		return remote->pushurl ? remote->pushurl : remote->url;
	}

	return NULL;
}

int git_remote_connect(git_remote *remote, git_direction direction)
{
	git_transport *t;
	const char *url;
	int flags = GIT_TRANSPORTFLAGS_NONE;

	assert(remote);

	t = remote->transport;

	url = git_remote__urlfordirection(remote, direction);
	if (url == NULL )
		return -1;

	/* A transport could have been supplied in advance with
	 * git_remote_set_transport */
	if (!t && git_transport_new(&t, remote, url) < 0)
		return -1;

	if (t->set_callbacks &&
		t->set_callbacks(t, remote->callbacks.progress, NULL, remote->callbacks.payload) < 0)
		goto on_error;

	if (!remote->check_cert)
		flags |= GIT_TRANSPORTFLAGS_NO_CHECK_CERT;

	if (t->connect(t, url, remote->cred_acquire_cb, remote->cred_acquire_payload, direction, flags) < 0)
		goto on_error;

	remote->transport = t;

	return 0;

on_error:
	t->free(t);

	if (t == remote->transport)
		remote->transport = NULL;

	return -1;
}

int git_remote_ls(git_remote *remote, git_headlist_cb list_cb, void *payload)
{
	assert(remote);

	return remote->transport->ls(remote->transport, list_cb, payload);
}

int git_remote__get_http_proxy(git_remote *remote, bool use_ssl, char **proxy_url)
{
	git_config *cfg;
	const char *val;

	assert(remote);

	if (!proxy_url || !remote->repo)
		return -1;

	*proxy_url = NULL;

	if (git_repository_config__weakptr(&cfg, remote->repo) < 0)
		return -1;

	/* Go through the possible sources for proxy configuration, from most specific
	 * to least specific. */

	/* remote.<name>.proxy config setting */
	if (remote->name && 0 != *(remote->name)) {
		git_buf buf = GIT_BUF_INIT;

		if (git_buf_printf(&buf, "remote.%s.proxy", remote->name) < 0)
			return -1;

		if (!git_config_get_string(&val, cfg, git_buf_cstr(&buf)) &&
			val && ('\0' != *val)) {
			git_buf_free(&buf);

			*proxy_url = git__strdup(val);
			GITERR_CHECK_ALLOC(*proxy_url);
			return 0;
		}

		git_buf_free(&buf);
	}

	/* http.proxy config setting */
	if (!git_config_get_string(&val, cfg, "http.proxy") &&
		val && ('\0' != *val)) {
		*proxy_url = git__strdup(val);
		GITERR_CHECK_ALLOC(*proxy_url);
		return 0;
	}

	/* HTTP_PROXY / HTTPS_PROXY environment variables */
	val = use_ssl ? getenv("HTTPS_PROXY") : getenv("HTTP_PROXY");

	if (val && ('\0' != *val)) {
		*proxy_url = git__strdup(val);
		GITERR_CHECK_ALLOC(*proxy_url);
		return 0;
	}

	return 0;
}

static int store_refs(git_remote_head *head, void *payload)
{
	git_vector *refs = (git_vector *)payload;

	return git_vector_insert(refs, head);
}

static int dwim_refspecs(git_vector *refspecs, git_vector *refs)
{
	git_buf buf = GIT_BUF_INIT;
	git_refspec *spec;
	size_t i, j, pos;
	git_remote_head key;

	const char* formatters[] = {
		GIT_REFS_DIR "%s",
		GIT_REFS_TAGS_DIR "%s",
		GIT_REFS_HEADS_DIR "%s",
		NULL
	};

	git_vector_foreach(refspecs, i, spec) {
		if (spec->dwim)
			continue;

		/* shorthand on the lhs */
		if (git__prefixcmp(spec->src, GIT_REFS_DIR)) {
			for (j = 0; formatters[j]; j++) {
				git_buf_clear(&buf);
				if (git_buf_printf(&buf, formatters[j], spec->src) < 0)
					return -1;

				key.name = (char *) git_buf_cstr(&buf);
				if (!git_vector_search(&pos, refs, &key)) {
					/* we found something to match the shorthand, set src to that */
					git__free(spec->src);
					spec->src = git_buf_detach(&buf);
				}
			}
		}

		if (spec->dst && git__prefixcmp(spec->dst, GIT_REFS_DIR)) {
			/* if it starts with "remotes" then we just prepend "refs/" */
			if (!git__prefixcmp(spec->dst, "remotes/")) {
				git_buf_puts(&buf, GIT_REFS_DIR);
			} else {
				git_buf_puts(&buf, GIT_REFS_HEADS_DIR);
			}

			if (git_buf_puts(&buf, spec->dst) < 0)
				return -1;

			git__free(spec->dst);
			spec->dst = git_buf_detach(&buf);
		}

		spec->dwim = 1;
	}

	git_buf_free(&buf);
	return 0;
}

static int remote_head_cmp(const void *_a, const void *_b)
{
	const git_remote_head *a = (git_remote_head *) _a;
	const git_remote_head *b = (git_remote_head *) _b;

	return git__strcmp_cb(a->name, b->name);
}

int git_remote_download(
		git_remote *remote,
		git_transfer_progress_callback progress_cb,
		void *progress_payload)
{
	int error;
	git_vector refs;

	assert(remote);

	if (git_vector_init(&refs, 16, remote_head_cmp) < 0)
		return -1;

	if (git_remote_ls(remote, store_refs, &refs) < 0) {
		return -1;
	}

	error = dwim_refspecs(&remote->refspecs, &refs);
	git_vector_free(&refs);
	if (error < 0)
		return -1;

	if ((error = git_fetch_negotiate(remote)) < 0)
		return error;

	return git_fetch_download_pack(remote, progress_cb, progress_payload);
}

static int remote_head_for_fetchspec_src(git_remote_head **out, git_vector *update_heads, const char *fetchspec_src)
{
	unsigned int i;
	git_remote_head *remote_ref;

	assert(update_heads && fetchspec_src);

	*out = NULL;

	git_vector_foreach(update_heads, i, remote_ref) {
		if (strcmp(remote_ref->name, fetchspec_src) == 0) {
			*out = remote_ref;
			break;
		}
	}

	return 0;
}

static int remote_head_for_ref(git_remote_head **out, git_refspec *spec, git_vector *update_heads, git_reference *ref)
{
	git_reference *resolved_ref = NULL;
	git_reference *tracking_ref = NULL;
	git_buf remote_name = GIT_BUF_INIT;
	int error = 0;

	assert(out && spec && ref);

	*out = NULL;

	if ((error = git_reference_resolve(&resolved_ref, ref)) < 0 ||
		(!git_reference_is_branch(resolved_ref)) ||
		(error = git_branch_upstream(&tracking_ref, resolved_ref)) < 0 ||
		(error = git_refspec_transform_l(&remote_name, spec, git_reference_name(tracking_ref))) < 0) {
		/* Not an error if HEAD is orphaned or no tracking branch */
		if (error == GIT_ENOTFOUND)
			error = 0;

		goto cleanup;
	}

	error = remote_head_for_fetchspec_src(out, update_heads, git_buf_cstr(&remote_name));

cleanup:
	git_reference_free(tracking_ref);
	git_reference_free(resolved_ref);
	git_buf_free(&remote_name);
	return error;
}

static int git_remote_write_fetchhead(git_remote *remote, git_refspec *spec, git_vector *update_heads)
{
	git_reference *head_ref = NULL;
	git_fetchhead_ref *fetchhead_ref;
	git_remote_head *remote_ref, *merge_remote_ref;
	git_vector fetchhead_refs;
	bool include_all_fetchheads;
	unsigned int i = 0;
	int error = 0;

	assert(remote);

	/* no heads, nothing to do */
	if (update_heads->length == 0)
		return 0;

	if (git_vector_init(&fetchhead_refs, update_heads->length, git_fetchhead_ref_cmp) < 0)
		return -1;

	/* Iff refspec is * (but not subdir slash star), include tags */
	include_all_fetchheads = (strcmp(GIT_REFS_HEADS_DIR "*", git_refspec_src(spec)) == 0);

	/* Determine what to merge: if refspec was a wildcard, just use HEAD */
	if (git_refspec_is_wildcard(spec)) {
		if ((error = git_reference_lookup(&head_ref, remote->repo, GIT_HEAD_FILE)) < 0 ||
			(error = remote_head_for_ref(&merge_remote_ref, spec, update_heads, head_ref)) < 0)
				goto cleanup;
	} else {
		/* If we're fetching a single refspec, that's the only thing that should be in FETCH_HEAD. */
		if ((error = remote_head_for_fetchspec_src(&merge_remote_ref, update_heads, git_refspec_src(spec))) < 0)
			goto cleanup;
	}

	/* Create the FETCH_HEAD file */
	git_vector_foreach(update_heads, i, remote_ref) {
		int merge_this_fetchhead = (merge_remote_ref == remote_ref);

		if (!include_all_fetchheads &&
			!git_refspec_src_matches(spec, remote_ref->name) &&
			!merge_this_fetchhead)
			continue;

		if (git_fetchhead_ref_create(&fetchhead_ref,
			&remote_ref->oid,
			merge_this_fetchhead,
			remote_ref->name,
			git_remote_url(remote)) < 0)
			goto cleanup;

		if (git_vector_insert(&fetchhead_refs, fetchhead_ref) < 0)
			goto cleanup;
	}

	git_fetchhead_write(remote->repo, &fetchhead_refs);

cleanup:
	for (i = 0; i < fetchhead_refs.length; ++i)
		git_fetchhead_ref_free(fetchhead_refs.contents[i]);

	git_vector_free(&fetchhead_refs);
	git_reference_free(head_ref);

	return error;
}

static int update_tips_for_spec(git_remote *remote, git_refspec *spec, git_vector *refs)
{
	int error = 0, autotag;
	unsigned int i = 0;
	git_buf refname = GIT_BUF_INIT;
	git_oid old;
	git_odb *odb;
	git_remote_head *head;
	git_reference *ref;
	git_refspec tagspec;
	git_vector update_heads;

	assert(remote);

	if (git_repository_odb__weakptr(&odb, remote->repo) < 0)
		return -1;

	if (git_refspec__parse(&tagspec, GIT_REFSPEC_TAGS, true) < 0)
		return -1;

	/* Make a copy of the transport's refs */
	if (git_vector_init(&update_heads, 16, NULL) < 0)
		return -1;

	/* Let's go find HEAD, if it exists. Check only the first ref in the vector. */
	if (refs->length > 0) {
		head = git_vector_get(refs, 0);

		if (!strcmp(head->name, GIT_HEAD_FILE))	{
			if (git_reference_create(&ref, remote->repo, GIT_FETCH_HEAD_FILE, &head->oid, 1) < 0)
				goto on_error;

			i = 1;
			git_reference_free(ref);
		}
	}

	for (; i < refs->length; ++i) {
		head = git_vector_get(refs, i);
		autotag = 0;

		/* Ignore malformed ref names (which also saves us from tag^{} */
		if (!git_reference_is_valid_name(head->name))
			continue;

		if (git_refspec_src_matches(spec, head->name) && spec->dst) {
			if (git_refspec_transform_r(&refname, spec, head->name) < 0)
				goto on_error;
		} else if (remote->download_tags != GIT_REMOTE_DOWNLOAD_TAGS_NONE) {

			if (remote->download_tags != GIT_REMOTE_DOWNLOAD_TAGS_ALL)
				autotag = 1;

			if (!git_refspec_src_matches(&tagspec, head->name))
				continue;

			git_buf_clear(&refname);
			if (git_buf_puts(&refname, head->name) < 0)
				goto on_error;
		} else {
			continue;
		}

		if (autotag && !git_odb_exists(odb, &head->oid))
			continue;

		if (git_vector_insert(&update_heads, head) < 0)
			goto on_error;

		error = git_reference_name_to_id(&old, remote->repo, refname.ptr);
		if (error < 0 && error != GIT_ENOTFOUND)
			goto on_error;

		if (error == GIT_ENOTFOUND)
			memset(&old, 0, GIT_OID_RAWSZ);

		if (!git_oid__cmp(&old, &head->oid))
			continue;

		/* In autotag mode, don't overwrite any locally-existing tags */
		error = git_reference_create(&ref, remote->repo, refname.ptr, &head->oid, !autotag);
		if (error < 0 && error != GIT_EEXISTS)
			goto on_error;

		git_reference_free(ref);

		if (remote->callbacks.update_tips != NULL) {
			if (remote->callbacks.update_tips(refname.ptr, &old, &head->oid, remote->callbacks.payload) < 0)
				goto on_error;
		}
	}

	if (git_remote_update_fetchhead(remote) &&
	    (error = git_remote_write_fetchhead(remote, spec, &update_heads)) < 0)
		goto on_error;

	git_vector_free(&update_heads);
	git_refspec__free(&tagspec);
	git_buf_free(&refname);
	return 0;

on_error:
	git_vector_free(&update_heads);
	git_refspec__free(&tagspec);
	git_buf_free(&refname);
	return -1;

}

int git_remote_update_tips(git_remote *remote)
{
	git_refspec *spec, tagspec;
	git_vector refs;
	int error;
	size_t i;


	if (git_refspec__parse(&tagspec, GIT_REFSPEC_TAGS, true) < 0)
		return -1;

	if (git_vector_init(&refs, 16, NULL) < 0)
		return -1;

	if ((error = git_remote_ls(remote, store_refs, &refs)) < 0)
		goto out;

	if (remote->download_tags == GIT_REMOTE_DOWNLOAD_TAGS_ALL) {
		error = update_tips_for_spec(remote, &tagspec, &refs);
		goto out;
	}

	git_vector_foreach(&remote->refspecs, i, spec) {
		if (spec->push)
			continue;

		if ((error = update_tips_for_spec(remote, spec, &refs)) < 0)
			goto out;
	}

out:
	git_refspec__free(&tagspec);
	git_vector_free(&refs);
	return error;
}

int git_remote_connected(git_remote *remote)
{
	assert(remote);

	if (!remote->transport || !remote->transport->is_connected)
		return 0;

	/* Ask the transport if it's connected. */
	return remote->transport->is_connected(remote->transport);
}

void git_remote_stop(git_remote *remote)
{
	assert(remote);

	if (remote->transport && remote->transport->cancel)
		remote->transport->cancel(remote->transport);
}

void git_remote_disconnect(git_remote *remote)
{
	assert(remote);

	if (git_remote_connected(remote))
		remote->transport->close(remote->transport);
}

void git_remote_free(git_remote *remote)
{
	git_refspec *spec;
	size_t i;

	if (remote == NULL)
		return;

	if (remote->transport != NULL) {
		git_remote_disconnect(remote);

		remote->transport->free(remote->transport);
		remote->transport = NULL;
	}

	git_vector_free(&remote->refs);

	git_vector_foreach(&remote->refspecs, i, spec) {
		git_refspec__free(spec);
		git__free(spec);
	}
	git_vector_free(&remote->refspecs);

	git__free(remote->url);
	git__free(remote->pushurl);
	git__free(remote->name);
	git__free(remote);
}

struct cb_data {
	git_vector *list;
	regex_t *preg;
};

static int remote_list_cb(const git_config_entry *entry, void *data_)
{
	struct cb_data *data = (struct cb_data *)data_;
	size_t nmatch = 2;
	regmatch_t pmatch[2];
	const char *name = entry->name;

	if (!regexec(data->preg, name, nmatch, pmatch, 0)) {
		char *remote_name = git__strndup(&name[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
		GITERR_CHECK_ALLOC(remote_name);

		if (git_vector_insert(data->list, remote_name) < 0)
			return -1;
	}

	return 0;
}

int git_remote_list(git_strarray *remotes_list, git_repository *repo)
{
	git_config *cfg;
	git_vector list;
	regex_t preg;
	struct cb_data data;
	int error;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	if (git_vector_init(&list, 4, NULL) < 0)
		return -1;

	if (regcomp(&preg, "^remote\\.(.*)\\.url$", REG_EXTENDED) < 0) {
		giterr_set(GITERR_OS, "Remote catch regex failed to compile");
		return -1;
	}

	data.list = &list;
	data.preg = &preg;
	error = git_config_foreach(cfg, remote_list_cb, &data);
	regfree(&preg);
	if (error < 0) {
		size_t i;
		char *elem;
		git_vector_foreach(&list, i, elem) {
			git__free(elem);
		}

		git_vector_free(&list);

		/* cb error is converted to GIT_EUSER by git_config_foreach */
		if (error == GIT_EUSER)
			error = -1;

		return error;
	}

	remotes_list->strings = (char **)list.contents;
	remotes_list->count = list.length;

	return 0;
}

void git_remote_check_cert(git_remote *remote, int check)
{
	assert(remote);

	remote->check_cert = check;
}

int git_remote_set_callbacks(git_remote *remote, git_remote_callbacks *callbacks)
{
	assert(remote && callbacks);

	GITERR_CHECK_VERSION(callbacks, GIT_REMOTE_CALLBACKS_VERSION, "git_remote_callbacks");

	memcpy(&remote->callbacks, callbacks, sizeof(git_remote_callbacks));

	if (remote->transport && remote->transport->set_callbacks)
		remote->transport->set_callbacks(remote->transport,
			remote->callbacks.progress,
			NULL,
			remote->callbacks.payload);

	return 0;
}

void git_remote_set_cred_acquire_cb(
	git_remote *remote,
	git_cred_acquire_cb cred_acquire_cb,
	void *payload)
{
	assert(remote);

	remote->cred_acquire_cb = cred_acquire_cb;
	remote->cred_acquire_payload = payload;
}

int git_remote_set_transport(git_remote *remote, git_transport *transport)
{
	assert(remote && transport);

	GITERR_CHECK_VERSION(transport, GIT_TRANSPORT_VERSION, "git_transport");

	if (remote->transport) {
		giterr_set(GITERR_NET, "A transport is already bound to this remote");
		return -1;
	}

	remote->transport = transport;
	return 0;
}

const git_transfer_progress* git_remote_stats(git_remote *remote)
{
	assert(remote);
	return &remote->stats;
}

git_remote_autotag_option_t git_remote_autotag(git_remote *remote)
{
	return remote->download_tags;
}

void git_remote_set_autotag(git_remote *remote, git_remote_autotag_option_t value)
{
	remote->download_tags = value;
}

static int rename_remote_config_section(
	git_repository *repo,
	const char *old_name,
	const char *new_name)
{
	git_buf old_section_name = GIT_BUF_INIT,
		new_section_name = GIT_BUF_INIT;
	int error = -1;

	if (git_buf_printf(&old_section_name, "remote.%s", old_name) < 0)
		goto cleanup;

	if (git_buf_printf(&new_section_name, "remote.%s", new_name) < 0)
		goto cleanup;

	error = git_config_rename_section(
		repo,
		git_buf_cstr(&old_section_name),
		git_buf_cstr(&new_section_name));

cleanup:
	git_buf_free(&old_section_name);
	git_buf_free(&new_section_name);

	return error;
}

struct update_data
{
	git_config *config;
	const char *old_remote_name;
	const char *new_remote_name;
};

static int update_config_entries_cb(
	const git_config_entry *entry,
	void *payload)
{
	struct update_data *data = (struct update_data *)payload;

	if (strcmp(entry->value, data->old_remote_name))
		return 0;

	return git_config_set_string(
		data->config,
		entry->name,
		data->new_remote_name);
}

static int update_branch_remote_config_entry(
	git_repository *repo,
	const char *old_name,
	const char *new_name)
{
	git_config *config;
	struct update_data data;

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	data.config = config;
	data.old_remote_name = old_name;
	data.new_remote_name = new_name;

	return git_config_foreach_match(
		config,
		"branch\\..+\\.remote",
		update_config_entries_cb, &data);
}

static int rename_one_remote_reference(
	git_repository *repo,
	const char *reference_name,
	const char *old_remote_name,
	const char *new_remote_name)
{
	int error = -1;
	git_buf new_name = GIT_BUF_INIT;
	git_reference *reference = NULL;
	git_reference *newref = NULL;

	if (git_buf_printf(
		&new_name,
		GIT_REFS_REMOTES_DIR "%s%s",
		new_remote_name,
		reference_name + strlen(GIT_REFS_REMOTES_DIR) + strlen(old_remote_name)) < 0)
			return -1;

	if (git_reference_lookup(&reference, repo, reference_name) < 0)
		goto cleanup;

	error = git_reference_rename(&newref, reference, git_buf_cstr(&new_name), 0);
	git_reference_free(reference);

cleanup:
	git_reference_free(newref);
	git_buf_free(&new_name);
	return error;
}

static int rename_remote_references(
	git_repository *repo,
	const char *old_name,
	const char *new_name)
{
	git_vector refnames;
	int error = -1;
	unsigned int i;
	char *name;
	const char *refname;
	git_reference_iterator *iter;

	if (git_vector_init(&refnames, 8, NULL) < 0)
		return -1;

	if (git_reference_iterator_new(&iter, repo) < 0)
		goto cleanup;

	while ((error = git_reference_next(&refname, iter)) == 0) {
		if (git__prefixcmp(refname, GIT_REFS_REMOTES_DIR))
			continue;

		if ((error = git_vector_insert(&refnames, git__strdup(refname))) < 0)
			break;

	}

	git_reference_iterator_free(iter);
	if (error == GIT_ITEROVER)
		error = 0;
	else
		goto cleanup;

	git_vector_foreach(&refnames, i, name) {
		if ((error = rename_one_remote_reference(repo, name, old_name, new_name)) < 0)
			goto cleanup;
	}

	error = 0;
cleanup:
	git_vector_foreach(&refnames, i, name) {
		git__free(name);
	}

	git_vector_free(&refnames);
	return error;
}

static int rename_fetch_refspecs(
	git_remote *remote,
	const char *new_name,
	int (*callback)(const char *problematic_refspec, void *payload),
	void *payload)
{
	git_config *config;
	git_buf base = GIT_BUF_INIT, var = GIT_BUF_INIT, val = GIT_BUF_INIT;
	const git_refspec *spec;
	size_t i;
	int error = -1;

	if (git_buf_printf(&base, "+refs/heads/*:refs/remotes/%s/*", remote->name) < 0)
		goto cleanup;

	git_vector_foreach(&remote->refspecs, i, spec) {
		if (spec->push)
			continue;

		/* Every refspec is a problem refspec for an in-memory remote */
		if (!remote->name) {
			if (callback(spec->string, payload) < 0) {
				error = GIT_EUSER;
				goto cleanup;
			}

			continue;
		}

		/* Does the dst part of the refspec follow the extected standard format? */
		if (strcmp(git_buf_cstr(&base), spec->string)) {
			if (callback(spec->string, payload) < 0) {
				error = GIT_EUSER;
				goto cleanup;
			}

			continue;
		}

		/* If we do want to move it to the new section */
		if (git_buf_printf(&val, "+refs/heads/*:refs/remotes/%s/*", new_name) < 0)
			goto cleanup;

		if (git_buf_printf(&var, "remote.%s.fetch", new_name) < 0)
			goto cleanup;

		if (git_repository_config__weakptr(&config, remote->repo) < 0)
			goto cleanup;

		if (git_config_set_string(config, git_buf_cstr(&var), git_buf_cstr(&val)) < 0)
			goto cleanup;
	}

	error = 0;

cleanup:
	git_buf_free(&base);
	git_buf_free(&var);
	git_buf_free(&val);
	return error;
}

int git_remote_rename(
	git_remote *remote,
	const char *new_name,
	git_remote_rename_problem_cb callback,
	void *payload)
{
	int error;

	assert(remote && new_name);

	if (!remote->name) {
		giterr_set(GITERR_INVALID, "Can't rename an in-memory remote.");
		return GIT_EINVALIDSPEC;
	}

	if ((error = ensure_remote_name_is_valid(new_name)) < 0)
		return error;

	if (remote->repo) {
		if ((error = ensure_remote_doesnot_exist(remote->repo, new_name)) < 0)
			return error;

		if (!remote->name) {
			if ((error = rename_fetch_refspecs(
				remote,
				new_name,
				callback,
				payload)) < 0)
				return error;

			remote->name = git__strdup(new_name);

			if (!remote->name) return 0;
			return git_remote_save(remote);
		}

		if ((error = rename_remote_config_section(
			remote->repo,
			remote->name,
			new_name)) < 0)
				return error;

		if ((error = update_branch_remote_config_entry(
			remote->repo,
			remote->name,
			new_name)) < 0)
				return error;

		if ((error = rename_remote_references(
			remote->repo,
			remote->name,
			new_name)) < 0)
				return error;

		if ((error = rename_fetch_refspecs(
			remote,
			new_name,
			callback,
			payload)) < 0)
			return error;
	}

	git__free(remote->name);
	remote->name = git__strdup(new_name);

	return 0;
}

int git_remote_update_fetchhead(git_remote *remote)
{
	return remote->update_fetchhead;
}

void git_remote_set_update_fetchhead(git_remote *remote, int value)
{
	remote->update_fetchhead = value;
}

int git_remote_is_valid_name(
	const char *remote_name)
{
	git_buf buf = GIT_BUF_INIT;
	git_refspec refspec;
	int error = -1;

	if (!remote_name || *remote_name == '\0')
		return 0;

	git_buf_printf(&buf, "refs/heads/test:refs/remotes/%s/test", remote_name);
	error = git_refspec__parse(&refspec, git_buf_cstr(&buf), true);

	git_buf_free(&buf);
	git_refspec__free(&refspec);

	giterr_clear();
	return error == 0;
}

git_refspec *git_remote__matching_refspec(git_remote *remote, const char *refname)
{
	git_refspec *spec;
	size_t i;

	git_vector_foreach(&remote->refspecs, i, spec) {
		if (spec->push)
			continue;

		if (git_refspec_src_matches(spec, refname))
			return spec;
	}

	return NULL;
}

git_refspec *git_remote__matching_dst_refspec(git_remote *remote, const char *refname)
{
	git_refspec *spec;
	size_t i;

	git_vector_foreach(&remote->refspecs, i, spec) {
		if (spec->push)
			continue;

		if (git_refspec_dst_matches(spec, refname))
			return spec;
	}

	return NULL;
}

void git_remote_clear_refspecs(git_remote *remote)
{
	git_refspec *spec;
	size_t i;

	git_vector_foreach(&remote->refspecs, i, spec) {
		git_refspec__free(spec);
		git__free(spec);
	}
	git_vector_clear(&remote->refspecs);
}

int git_remote_add_fetch(git_remote *remote, const char *refspec)
{
	return add_refspec(remote, refspec, true);
}

int git_remote_add_push(git_remote *remote, const char *refspec)
{
	return add_refspec(remote, refspec, false);
}

static int copy_refspecs(git_strarray *array, git_remote *remote, int push)
{
	size_t i;
	git_vector refspecs;
	git_refspec *spec;
	char *dup;

	if (git_vector_init(&refspecs, remote->refspecs.length, NULL) < 0)
		return -1;

	git_vector_foreach(&remote->refspecs, i, spec) {
		if (spec->push != push)
			continue;

		if ((dup = git__strdup(spec->string)) == NULL)
			goto on_error;

		if (git_vector_insert(&refspecs, dup) < 0) {
			git__free(dup);
			goto on_error;
		}
	}

	array->strings = (char **)refspecs.contents;
	array->count = refspecs.length;

	return 0;

on_error:
	git_vector_foreach(&refspecs, i, dup)
		git__free(dup);
	git_vector_free(&refspecs);

	return -1;
}

int git_remote_get_fetch_refspecs(git_strarray *array, git_remote *remote)
{
	return copy_refspecs(array, remote, false);
}

int git_remote_get_push_refspecs(git_strarray *array, git_remote *remote)
{
	return copy_refspecs(array, remote, true);
}

size_t git_remote_refspec_count(git_remote *remote)
{
	return remote->refspecs.length;
}

const git_refspec *git_remote_get_refspec(git_remote *remote, size_t n)
{
	return git_vector_get(&remote->refspecs, n);
}

int git_remote_remove_refspec(git_remote *remote, size_t n)
{
	git_refspec *spec;

	assert(remote);

	spec = git_vector_get(&remote->refspecs, n);
	if (spec) {
		git_refspec__free(spec);
		git__free(spec);
	}

	return git_vector_remove(&remote->refspecs, n);
}
