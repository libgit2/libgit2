/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/remote.h"
#include "git2/config.h"
#include "git2/types.h"

#include "config.h"
#include "repository.h"
#include "remote.h"
#include "fetch.h"
#include "refs.h"

#include <regex.h>

static int refspec_parse(git_refspec *refspec, const char *str)
{
	char *delim;

	memset(refspec, 0x0, sizeof(git_refspec));

	if (*str == '+') {
		refspec->force = 1;
		str++;
	}

	delim = strchr(str, ':');
	if (delim == NULL) {
		giterr_set(GITERR_NET, "Invalid refspec, missing ':'");
		return -1;
	}

	refspec->src = git__strndup(str, delim - str);
	GITERR_CHECK_ALLOC(refspec->src);

	refspec->dst = git__strdup(delim + 1);
	GITERR_CHECK_ALLOC(refspec->dst);

	return 0;
}

static int parse_remote_refspec(git_config *cfg, git_refspec *refspec, const char *var)
{
	int error;
	const char *val;

	if ((error = git_config_get_string(&val, cfg, var)) < 0)
		return error;

	return refspec_parse(refspec, val);
}

int git_remote_new(git_remote **out, git_repository *repo, const char *name, const char *url, const char *fetch)
{
	git_remote *remote;

	/* name is optional */
	assert(out && repo && url);

	remote = git__malloc(sizeof(git_remote));
	GITERR_CHECK_ALLOC(remote);

	memset(remote, 0x0, sizeof(git_remote));
	remote->repo = repo;

	if (git_vector_init(&remote->refs, 32, NULL) < 0)
		return -1;

	remote->url = git__strdup(url);
	GITERR_CHECK_ALLOC(remote->url);

	if (name != NULL) {
		remote->name = git__strdup(name);
		GITERR_CHECK_ALLOC(remote->name);
	}

	if (fetch != NULL) {
		if (refspec_parse(&remote->fetch, fetch) < 0)
			goto on_error;
	}

	*out = remote;
	return 0;

on_error:
	git_remote_free(remote);
	return -1;
}

int git_remote_load(git_remote **out, git_repository *repo, const char *name)
{
	git_remote *remote;
	git_buf buf = GIT_BUF_INIT;
	const char *val;
	int error = 0;
	git_config *config;

	assert(out && repo && name);

	if (git_repository_config__weakptr(&config, repo) < 0)
		return -1;

	remote = git__malloc(sizeof(git_remote));
	GITERR_CHECK_ALLOC(remote);

	memset(remote, 0x0, sizeof(git_remote));
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

	remote->repo = repo;
	remote->url = git__strdup(val);
	GITERR_CHECK_ALLOC(remote->url);

	git_buf_clear(&buf);
	if (git_buf_printf(&buf, "remote.%s.fetch", name) < 0) {
		error = -1;
		goto cleanup;
	}

	error = parse_remote_refspec(config, &remote->fetch, git_buf_cstr(&buf));
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

	error = parse_remote_refspec(config, &remote->push, git_buf_cstr(&buf));
	if (error == GIT_ENOTFOUND)
		error = 0;

	if (error < 0) {
		error = -1;
		goto cleanup;
	}

	*out = remote;

cleanup:
	git_buf_free(&buf);

	if (error < 0)
		git_remote_free(remote);

	return error;
}

int git_remote_save(const git_remote *remote)
{
	git_config *config;
	git_buf buf = GIT_BUF_INIT, value = GIT_BUF_INIT;

	if (git_repository_config__weakptr(&config, remote->repo) < 0)
		return -1;

	if (git_buf_printf(&buf, "remote.%s.%s", remote->name, "url") < 0)
		return -1;

	if (git_config_set_string(config, git_buf_cstr(&buf), remote->url) < 0) {
		git_buf_free(&buf);
		return -1;
	}

	if (remote->fetch.src != NULL && remote->fetch.dst != NULL) {
		git_buf_clear(&buf);
		git_buf_clear(&value);
		git_buf_printf(&buf, "remote.%s.fetch", remote->name);
		git_buf_printf(&value, "%s:%s", remote->fetch.src, remote->fetch.dst);
		if (git_buf_oom(&buf) || git_buf_oom(&value))
			return -1;

		if (git_config_set_string(config, git_buf_cstr(&buf), git_buf_cstr(&value)) < 0)
			goto on_error;
	}

	if (remote->push.src != NULL && remote->push.dst != NULL) {
		git_buf_clear(&buf);
		git_buf_clear(&value);
		git_buf_printf(&buf, "remote.%s.push", remote->name);
		git_buf_printf(&value, "%s:%s", remote->push.src, remote->push.dst);
		if (git_buf_oom(&buf) || git_buf_oom(&value))
			return -1;

		if (git_config_set_string(config, git_buf_cstr(&buf), git_buf_cstr(&value)) < 0)
			goto on_error;
	}

	git_buf_free(&buf);
	git_buf_free(&value);

	return 0;

on_error:
	git_buf_free(&buf);
	git_buf_free(&value);
	return -1;
}

const char *git_remote_name(git_remote *remote)
{
	assert(remote);
	return remote->name;
}

const char *git_remote_url(git_remote *remote)
{
	assert(remote);
	return remote->url;
}

int git_remote_set_fetchspec(git_remote *remote, const char *spec)
{
	git_refspec refspec;

	assert(remote && spec);

	if (refspec_parse(&refspec, spec) < 0)
		return -1;

	git__free(remote->fetch.src);
	git__free(remote->fetch.dst);
	remote->fetch.src = refspec.src;
	remote->fetch.dst = refspec.dst;

	return 0;
}

const git_refspec *git_remote_fetchspec(git_remote *remote)
{
	assert(remote);
	return &remote->fetch;
}

int git_remote_set_pushspec(git_remote *remote, const char *spec)
{
	git_refspec refspec;

	assert(remote && spec);

	if (refspec_parse(&refspec, spec) < 0)
		return -1;

	git__free(remote->push.src);
	git__free(remote->push.dst);
	remote->push.src = refspec.src;
	remote->push.dst = refspec.dst;

	return 0;
}

const git_refspec *git_remote_pushspec(git_remote *remote)
{
	assert(remote);
	return &remote->push;
}

int git_remote_connect(git_remote *remote, int direction)
{
	git_transport *t;

	assert(remote);

	if (git_transport_new(&t, remote->url) < 0)
		return -1;

	if (t->connect(t, direction) < 0) {
		goto on_error;
	}

	remote->transport = t;

	return 0;

on_error:
	t->free(t);
	return -1;
}

int git_remote_ls(git_remote *remote, git_headlist_cb list_cb, void *payload)
{
	assert(remote);

	if (!remote->transport || !remote->transport->connected) {
		giterr_set(GITERR_NET, "The remote is not connected");
		return -1;
	}

	return remote->transport->ls(remote->transport, list_cb, payload);
}

int git_remote_download(git_remote *remote, git_off_t *bytes, git_indexer_stats *stats)
{
	int error;

	assert(remote && bytes && stats);

	if ((error = git_fetch_negotiate(remote)) < 0)
		return error;

	return git_fetch_download_pack(remote, bytes, stats);
}

int git_remote_update_tips(git_remote *remote, int (*cb)(const char *refname, const git_oid *a, const git_oid *b))
{
	int error = 0;
	unsigned int i = 0;
	git_buf refname = GIT_BUF_INIT;
	git_oid old;
	git_vector *refs = &remote->refs;
	git_remote_head *head;
	git_reference *ref;
	struct git_refspec *spec = &remote->fetch;

	assert(remote);

	if (refs->length == 0)
		return 0;

	/* HEAD is only allowed to be the first in the list */
	head = refs->contents[0];
	if (!strcmp(head->name, GIT_HEAD_FILE)) {
		if (git_reference_create_oid(&ref, remote->repo, GIT_FETCH_HEAD_FILE, &head->oid, 1) < 0)
			return -1;

		i = 1;
		git_reference_free(ref);
	}

	for (; i < refs->length; ++i) {
		head = refs->contents[i];

		if (git_refspec_transform_r(&refname, spec, head->name) < 0)
			goto on_error;

		error = git_reference_name_to_oid(&old, remote->repo, refname.ptr);
		if (error < 0 && error != GIT_ENOTFOUND)
			goto on_error;

		if (error == GIT_ENOTFOUND)
			memset(&old, 0, GIT_OID_RAWSZ);

		if (!git_oid_cmp(&old, &head->oid))
			continue;

		if (git_reference_create_oid(&ref, remote->repo, refname.ptr, &head->oid, 1) < 0)
			break;

		git_reference_free(ref);

		if (cb != NULL) {
			if (cb(refname.ptr, &old, &head->oid) < 0)
				goto on_error;
		}
	}

	git_buf_free(&refname);
	return 0;

on_error:
	git_buf_free(&refname);
	return -1;

}

int git_remote_connected(git_remote *remote)
{
	assert(remote);
	return remote->transport == NULL ? 0 : remote->transport->connected;
}

void git_remote_disconnect(git_remote *remote)
{
	assert(remote);

	if (remote->transport != NULL && remote->transport->connected)
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

	git__free(remote->fetch.src);
	git__free(remote->fetch.dst);
	git__free(remote->push.src);
	git__free(remote->push.dst);
	git__free(remote->url);
	git__free(remote->name);
	git__free(remote);
}

struct cb_data {
	git_vector *list;
	regex_t *preg;
};

static int remote_list_cb(const char *name, const char *value, void *data_)
{
	struct cb_data *data = (struct cb_data *)data_;
	size_t nmatch = 2;
	regmatch_t pmatch[2];
	GIT_UNUSED(value);

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
		return error;
	}

	remotes_list->strings = (char **)list.contents;
	remotes_list->count = list.length;

	return 0;
}

int git_remote_add(git_remote **out, git_repository *repo, const char *name, const char *url)
{
	git_buf buf = GIT_BUF_INIT;

	if (git_buf_printf(&buf, "refs/heads/*:refs/remotes/%s/*", name) < 0)
		return -1;

	if (git_remote_new(out, repo, name, url, git_buf_cstr(&buf)) < 0)
		goto on_error;

	git_buf_free(&buf);

	if (git_remote_save(*out) < 0)
		goto on_error;

	return 0;

on_error:
	git_buf_free(&buf);
	git_remote_free(*out);
	return -1;
}
