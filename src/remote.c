/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/config.h"
#include "git2/types.h"
#include "git2/oid.h"

#include "config.h"
#include "repository.h"
#include "remote.h"
#include "fetch.h"
#include "refs.h"
#include "refspec.h"
#include "fetchhead.h"

#include <regex.h>

static int parse_remote_refspec(git_config *cfg, git_refspec *refspec, const char *var, bool is_fetch)
{
	int error;
	const char *val;

	if ((error = git_config_get_string(&val, cfg, var)) < 0)
		return error;

	return git_refspec__parse(refspec, val, is_fetch);
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

	if (error == GIT_ENOTFOUND)
		error = 0;

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
		if (git_refspec__parse(&remote->fetch, fetch, true) < 0)
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

int git_remote_load(git_remote **out, git_repository *repo, const char *name)
{
	git_remote *remote;
	git_buf buf = GIT_BUF_INIT;
	const char *val;
	int error = 0;
	git_config *config;

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

	if (git_vector_init(&remote->refs, 32, NULL) < 0) {
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

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.pushurl", name) < 0) {
		error = -1;
		goto cleanup;
	}

	error = git_config_get_string(&val, config, git_buf_cstr(&buf));
	if (error == GIT_ENOTFOUND) {
		val = NULL;
		error = 0;
	}

	if (error < 0) {
		error = -1;
		goto cleanup;
	}

	if (val) {
		remote->pushurl = git__strdup(val);
		GITERR_CHECK_ALLOC(remote->pushurl);
	}

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.fetch", name) < 0) {
		error = -1;
		goto cleanup;
	}

	error = parse_remote_refspec(config, &remote->fetch, git_buf_cstr(&buf), true);
	if (error == GIT_ENOTFOUND)
		error = 0;

	if (error < 0) {
		error = -1;
		goto cleanup;
	}

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.push", name) < 0) {
		error = -1;
		goto cleanup;
	}

	error = parse_remote_refspec(config, &remote->push, git_buf_cstr(&buf), false);
	if (error == GIT_ENOTFOUND)
		error = 0;

	if (error < 0) {
		error = -1;
		goto cleanup;
	}

	if (download_tags_value(remote, config) < 0)
		goto cleanup;

	*out = remote;

cleanup:
	git_buf_free(&buf);

	if (error < 0)
		git_remote_free(remote);

	return error;
}

static int update_config_refspec(
	git_config *config,
	const char *remote_name,
	const git_refspec *refspec,
	int git_direction)
{
	git_buf name = GIT_BUF_INIT, value = GIT_BUF_INIT;
	int error = -1;

	if (refspec->src == NULL || refspec->dst == NULL)
		return 0;

	if (git_buf_printf(
		&name,
		"remote.%s.%s",
		remote_name,
		git_direction == GIT_DIRECTION_FETCH ? "fetch" : "push") < 0)
			goto cleanup;

	if (git_refspec__serialize(&value, refspec) < 0)
		goto cleanup;

	error = git_config_set_string(
		config,
		git_buf_cstr(&name),
		git_buf_cstr(&value));

cleanup:
	git_buf_free(&name);
	git_buf_free(&value);

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

	if (update_config_refspec(
		config,
		remote->name,
		&remote->fetch,
		GIT_DIRECTION_FETCH) < 0)
			goto on_error;

	if (update_config_refspec(
		config,
		remote->name,
		&remote->push,
		GIT_DIRECTION_PUSH) < 0)
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

int git_remote_set_fetchspec(git_remote *remote, const char *spec)
{
	git_refspec refspec;

	assert(remote && spec);

	if (git_refspec__parse(&refspec, spec, true) < 0)
		return -1;

	git_refspec__free(&remote->fetch);
	memcpy(&remote->fetch, &refspec, sizeof(git_refspec));

	return 0;
}

const git_refspec *git_remote_fetchspec(const git_remote *remote)
{
	assert(remote);
	return &remote->fetch;
}

int git_remote_set_pushspec(git_remote *remote, const char *spec)
{
	git_refspec refspec;

	assert(remote && spec);

	if (git_refspec__parse(&refspec, spec, false) < 0)
		return -1;

	git_refspec__free(&remote->push);
	remote->push.src = refspec.src;
	remote->push.dst = refspec.dst;

	return 0;
}

const git_refspec *git_remote_pushspec(const git_remote *remote)
{
	assert(remote);
	return &remote->push;
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

	if (!git_remote_connected(remote)) {
		giterr_set(GITERR_NET, "The remote is not connected");
		return -1;
	}

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

int git_remote_download(
		git_remote *remote,
		git_transfer_progress_callback progress_cb,
		void *progress_payload)
{
	int error;

	assert(remote);

	if ((error = git_fetch_negotiate(remote)) < 0)
		return error;

	return git_fetch_download_pack(remote, progress_cb, progress_payload);
}

static int update_tips_callback(git_remote_head *head, void *payload)
{
	git_vector *refs = (git_vector *)payload;

	return git_vector_insert(refs, head);
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

static int remote_head_for_ref(git_remote_head **out, git_remote *remote, git_vector *update_heads, git_reference *ref)
{
	git_reference *resolved_ref = NULL;
	git_reference *tracking_ref = NULL;
	git_buf remote_name = GIT_BUF_INIT;
	int error = 0;

	assert(out && remote && ref);

	*out = NULL;

	if ((error = git_reference_resolve(&resolved_ref, ref)) < 0 ||
		(!git_reference_is_branch(resolved_ref)) ||
		(error = git_branch_tracking(&tracking_ref, resolved_ref)) < 0 ||
		(error = git_refspec_transform_l(&remote_name, &remote->fetch, git_reference_name(tracking_ref))) < 0) {
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

static int git_remote_write_fetchhead(git_remote *remote, git_vector *update_heads)
{
	struct git_refspec *spec;
	git_reference *head_ref = NULL;
	git_fetchhead_ref *fetchhead_ref;
	git_remote_head *remote_ref, *merge_remote_ref;
	git_vector fetchhead_refs;
	bool include_all_fetchheads;
	unsigned int i = 0;
	int error = 0;

	assert(remote);

	spec = &remote->fetch;

	if (git_vector_init(&fetchhead_refs, update_heads->length, git_fetchhead_ref_cmp) < 0)
		return -1;

	/* Iff refspec is * (but not subdir slash star), include tags */
	include_all_fetchheads = (strcmp(GIT_REFS_HEADS_DIR "*", git_refspec_src(spec)) == 0);

	/* Determine what to merge: if refspec was a wildcard, just use HEAD */
	if (git_refspec_is_wildcard(spec)) {
		if ((error = git_reference_lookup(&head_ref, remote->repo, GIT_HEAD_FILE)) < 0 ||
			(error = remote_head_for_ref(&merge_remote_ref, remote, update_heads, head_ref)) < 0)
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

int git_remote_update_tips(git_remote *remote)
{
	int error = 0, autotag;
	unsigned int i = 0;
	git_buf refname = GIT_BUF_INIT;
	git_oid old;
	git_odb *odb;
	git_remote_head *head;
	git_reference *ref;
	struct git_refspec *spec;
	git_refspec tagspec;
	git_vector refs, update_heads;

	assert(remote);

	spec = &remote->fetch;
	
	if (git_repository_odb__weakptr(&odb, remote->repo) < 0)
		return -1;

	if (git_refspec__parse(&tagspec, GIT_REFSPEC_TAGS, true) < 0)
		return -1;

	/* Make a copy of the transport's refs */
	if (git_vector_init(&refs, 16, NULL) < 0 ||
		git_vector_init(&update_heads, 16, NULL) < 0)
		return -1;

	if (git_remote_ls(remote, update_tips_callback, &refs) < 0)
		goto on_error;

	/* Let's go find HEAD, if it exists. Check only the first ref in the vector. */
	if (refs.length > 0) {
		head = (git_remote_head *)refs.contents[0];

		if (!strcmp(head->name, GIT_HEAD_FILE))	{
			if (git_reference_create(&ref, remote->repo, GIT_FETCH_HEAD_FILE, &head->oid, 1) < 0)
				goto on_error;

			i = 1;
			git_reference_free(ref);
		}
	}

	for (; i < refs.length; ++i) {
		head = (git_remote_head *)refs.contents[i];
		autotag = 0;

		/* Ignore malformed ref names (which also saves us from tag^{} */
		if (!git_reference_is_valid_name(head->name))
			continue;

		if (git_refspec_src_matches(spec, head->name)) {
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

		if (!git_oid_cmp(&old, &head->oid))
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
		(error = git_remote_write_fetchhead(remote, &update_heads)) < 0)
		goto on_error;

	git_vector_free(&refs);
	git_vector_free(&update_heads);
	git_refspec__free(&tagspec);
	git_buf_free(&refname);
	return 0;

on_error:
	git_vector_free(&refs);
	git_vector_free(&update_heads);
	git_refspec__free(&tagspec);
	git_buf_free(&refname);
	return -1;

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
	if (remote == NULL)
		return;

	if (remote->transport != NULL) {
		git_remote_disconnect(remote);

		remote->transport->free(remote->transport);
		remote->transport = NULL;
	}

	git_vector_free(&remote->refs);

	git_refspec__free(&remote->fetch);
	git_refspec__free(&remote->push);
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

static int rename_cb(const char *ref, void *data)
{
	if (git__prefixcmp(ref, GIT_REFS_REMOTES_DIR))
		return 0;
	
	return git_vector_insert((git_vector *)data, git__strdup(ref));
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

	if (git_vector_init(&refnames, 8, NULL) < 0)
		goto cleanup;

	if (git_reference_foreach(
		repo,
		GIT_REF_LISTALL,
		rename_cb,
		&refnames) < 0)
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
	const git_refspec *fetch_refspec;
	git_buf dst_prefix = GIT_BUF_INIT, serialized = GIT_BUF_INIT;
	const char* pos;
	int error = -1;

	fetch_refspec = git_remote_fetchspec(remote);

	/* Is there a refspec to deal with? */
	if (fetch_refspec->src == NULL &&
		fetch_refspec->dst == NULL)
		return 0;

	if (git_refspec__serialize(&serialized, fetch_refspec) < 0)
		goto cleanup;

	/* Is it an in-memory remote? */
	if (!remote->name) {
		error = (callback(git_buf_cstr(&serialized), payload) < 0) ? GIT_EUSER : 0;
		goto cleanup;
	}

	if (git_buf_printf(&dst_prefix, ":refs/remotes/%s/", remote->name) < 0)
		goto cleanup;

	pos = strstr(git_buf_cstr(&serialized), git_buf_cstr(&dst_prefix));

	/* Does the dst part of the refspec follow the extected standard format? */
	if (!pos) {
		error = (callback(git_buf_cstr(&serialized), payload) < 0) ? GIT_EUSER : 0;
		goto cleanup;
	}

	if (git_buf_splice(
		&serialized,
		pos - git_buf_cstr(&serialized) + strlen(":refs/remotes/"),
		strlen(remote->name), new_name,
		strlen(new_name)) < 0)
			goto cleanup;

	git_refspec__free(&remote->fetch);

	if (git_refspec__parse(&remote->fetch, git_buf_cstr(&serialized), true) < 0)
		goto cleanup;

	if (git_repository_config__weakptr(&config, remote->repo) < 0)
		goto cleanup;

	error = update_config_refspec(config, new_name, &remote->fetch, GIT_DIRECTION_FETCH);

cleanup:
	git_buf_free(&serialized);
	git_buf_free(&dst_prefix);
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
