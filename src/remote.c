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
	if (delim == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse refspec. No ':'");

	refspec->src = git__strndup(str, delim - str);
	if (refspec->src == NULL)
		return GIT_ENOMEM;

	refspec->dst = git__strdup(delim + 1);
	if (refspec->dst == NULL) {
		git__free(refspec->src);
		refspec->src = NULL;
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

static int parse_remote_refspec(git_config *cfg, git_refspec *refspec, const char *var)
{
	const char *val;
	int error;

	error = git_config_get_string(cfg, var, &val);
	if (error < GIT_SUCCESS)
		return error;

	return refspec_parse(refspec, val);
}

int git_remote_new(git_remote **out, git_repository *repo, const char *url, const char *name)
{
	git_remote *remote;

	/* name is optional */
	assert(out && repo && url);

	remote = git__malloc(sizeof(git_remote));
	if (remote == NULL)
		return GIT_ENOMEM;

	memset(remote, 0x0, sizeof(git_remote));
	remote->repo = repo;

	if (git_vector_init(&remote->refs, 32, NULL) < 0) {
		git_remote_free(remote);
		return GIT_ENOMEM;
	}

	remote->url = git__strdup(url);
	if (remote->url == NULL) {
		git_remote_free(remote);
		return GIT_ENOMEM;
	}

	if (name != NULL) {
		remote->name = git__strdup(name);
		if (remote->name == NULL) {
			git_remote_free(remote);
			return GIT_ENOMEM;
		}
	}

	*out = remote;
	return GIT_SUCCESS;
}

int git_remote_load(git_remote **out, git_repository *repo, const char *name)
{
	git_remote *remote;
	git_buf buf = GIT_BUF_INIT;
	const char *val;
	int error;
	git_config *config;

	assert(out && repo && name);

	error = git_repository_config__weakptr(&config, repo);
	if (error < GIT_SUCCESS)
		return error;

	remote = git__malloc(sizeof(git_remote));
	if (remote == NULL)
		return GIT_ENOMEM;

	memset(remote, 0x0, sizeof(git_remote));
	remote->name = git__strdup(name);
	if (remote->name == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	if (git_vector_init(&remote->refs, 32, NULL) < 0) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	git_buf_printf(&buf, "remote.%s.url", name);
	if (git_buf_oom(&buf)) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = git_config_get_string(config, git_buf_cstr(&buf), &val);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Remote's url doesn't exist");
		goto cleanup;
	}

	remote->repo = repo;
	remote->url = git__strdup(val);
	if (remote->url == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	git_buf_clear(&buf);
	git_buf_printf(&buf, "remote.%s.fetch", name);
	if (git_buf_oom(&buf)) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = parse_remote_refspec(config, &remote->fetch, git_buf_cstr(&buf));
	if (error == GIT_ENOTFOUND)
		error = GIT_SUCCESS;

	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to get fetch refspec");
		goto cleanup;
	}

	git_buf_clear(&buf);
	git_buf_printf(&buf, "remote.%s.push", name);
	if (git_buf_oom(&buf)) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	error = parse_remote_refspec(config, &remote->push, git_buf_cstr(&buf));
	/* Not finding push is fine */
	if (error == GIT_ENOTFOUND)
		error = GIT_SUCCESS;

	if (error < GIT_SUCCESS)
		goto cleanup;

	*out = remote;

cleanup:
	git_buf_free(&buf);

	if (error < GIT_SUCCESS)
		git_remote_free(remote);

	return error;
}

int git_remote_save(const git_remote *remote)
{
	int error;
	git_config *config;
	git_buf buf = GIT_BUF_INIT, value = GIT_BUF_INIT;

	error = git_repository_config__weakptr(&config, remote->repo);
	if (error < GIT_SUCCESS)
		return error;

	git_buf_printf(&buf, "remote.%s.%s", remote->name, "url");
	if (git_buf_oom(&buf))
		return GIT_ENOMEM;

	error = git_config_set_string(config, git_buf_cstr(&buf), remote->url);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if (remote->fetch.src != NULL && remote->fetch.dst != NULL) {
		git_buf_clear(&buf);
		git_buf_clear(&value);
		git_buf_printf(&buf, "remote.%s.%s", remote->name, "fetch");
		git_buf_printf(&value, "%s:%s", remote->fetch.src, remote->fetch.dst);
		if (git_buf_oom(&buf) || git_buf_oom(&value))
			return GIT_ENOMEM;

		error = git_config_set_string(config, git_buf_cstr(&buf), git_buf_cstr(&value));
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	if (remote->push.src != NULL && remote->push.dst != NULL) {
		git_buf_clear(&buf);
		git_buf_clear(&value);
		git_buf_printf(&buf, "remote.%s.%s", remote->name, "push");
		git_buf_printf(&value, "%s:%s", remote->push.src, remote->push.dst);
		if (git_buf_oom(&buf) || git_buf_oom(&value))
			return GIT_ENOMEM;

		error = git_config_set_string(config, git_buf_cstr(&buf), git_buf_cstr(&value));
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

cleanup:
	git_buf_free(&buf);
	git_buf_free(&value);
	return error;
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
	int error;
	git_refspec refspec;

	assert(remote && spec);

	error = refspec_parse(&refspec, spec);
	if (error != GIT_SUCCESS)
		return error;

	git__free(remote->fetch.src);
	git__free(remote->fetch.dst);
	remote->fetch.src = refspec.src;
	remote->fetch.dst = refspec.dst;

	return GIT_SUCCESS;
}

const git_refspec *git_remote_fetchspec(git_remote *remote)
{
	assert(remote);
	return &remote->fetch;
}

int git_remote_set_pushspec(git_remote *remote, const char *spec)
{
	int error;
	git_refspec refspec;

	assert(remote && spec);

	error = refspec_parse(&refspec, spec);
	if (error != GIT_SUCCESS)
		return error;

	git__free(remote->push.src);
	git__free(remote->push.dst);
	remote->push.src = refspec.src;
	remote->push.dst = refspec.dst;

	return GIT_SUCCESS;
}

const git_refspec *git_remote_pushspec(git_remote *remote)
{
	assert(remote);
	return &remote->push;
}

int git_remote_connect(git_remote *remote, int direction)
{
	int error;
	git_transport *t;

	assert(remote);

	error = git_transport_new(&t, remote->url);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create transport");

	error = t->connect(t, direction);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to connect the transport");
		goto cleanup;
	}

	remote->transport = t;

cleanup:
	if (error < GIT_SUCCESS)
		t->free(t);

	return error;
}

int git_remote_ls(git_remote *remote, git_headlist_cb list_cb, void *payload)
{
	assert(remote);

	if (!remote->transport || !remote->transport->connected)
		return git__throw(GIT_ERROR, "The remote is not connected");

	return remote->transport->ls(remote->transport, list_cb, payload);
}

int git_remote_download(char **filename, git_remote *remote)
{
	int error;

	assert(filename && remote);

	if ((error = git_fetch_negotiate(remote)) < 0)
		return git__rethrow(error, "Error negotiating");

	return git_fetch_download_pack(filename, remote);
}

int git_remote_update_tips(git_remote *remote)
{
	int error = GIT_SUCCESS;
	unsigned int i = 0;
	git_buf refname = GIT_BUF_INIT;
	git_vector *refs = &remote->refs;
	git_remote_head *head;
	git_reference *ref;
	struct git_refspec *spec = &remote->fetch;

	assert(remote);

	if (refs->length == 0)
		return GIT_SUCCESS;

	/* HEAD is only allowed to be the first in the list */
	head = refs->contents[0];
	if (!strcmp(head->name, GIT_HEAD_FILE)) {
		error = git_reference_create_oid(&ref, remote->repo, GIT_FETCH_HEAD_FILE, &head->oid, 1);
		i = 1;
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to update FETCH_HEAD");

		git_reference_free(ref);
	}

	for (; i < refs->length; ++i) {
		head = refs->contents[i];

		error = git_refspec_transform_r(&refname, spec, head->name);
		if (error < GIT_SUCCESS)
			break;

		error = git_reference_create_oid(&ref, remote->repo, refname.ptr, &head->oid, 1);
		if (error < GIT_SUCCESS)
			break;

		git_reference_free(ref);
	}

	git_buf_free(&refname);

	return error;
}

int git_remote_connected(git_remote *remote)
{
	assert(remote);
	return remote->transport == NULL ? 0 : remote->transport->connected;
}

void git_remote_disconnect(git_remote *remote)
{
	assert(remote);

	if (remote->transport != NULL) {
		if (remote->transport->connected)
			remote->transport->close(remote->transport);

		remote->transport->free(remote->transport);
		remote->transport = NULL;
	}
}

void git_remote_free(git_remote *remote)
{
	if (remote == NULL)
		return;

	git__free(remote->fetch.src);
	git__free(remote->fetch.dst);
	git__free(remote->push.src);
	git__free(remote->push.dst);
	git__free(remote->url);
	git__free(remote->name);
	git_vector_free(&remote->refs);
	git_remote_disconnect(remote);
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
	int error;
	GIT_UNUSED(value);

	if (!regexec(data->preg, name, nmatch, pmatch, 0)) {
		char *remote_name = git__strndup(&name[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
		if (remote_name == NULL)
			return GIT_ENOMEM;

		error = git_vector_insert(data->list, remote_name);
		if (error < GIT_SUCCESS)
			return error;
	}

	return GIT_SUCCESS;
}

int git_remote_list(git_strarray *remotes_list, git_repository *repo)
{
	git_config *cfg;
	git_vector list;
	regex_t preg;
	struct cb_data data;
	int error;

	error = git_repository_config__weakptr(&cfg, repo);
	if (error < GIT_SUCCESS)
		return error;

	error = git_vector_init(&list, 4, NULL);
	if (error < GIT_SUCCESS)
		return error;

	error = regcomp(&preg, "^remote\\.(.*)\\.url$", REG_EXTENDED);
	if (error < 0)
		return GIT_EOSERR;

	data.list = &list;
	data.preg = &preg;
	error = git_config_foreach(cfg, remote_list_cb, &data);
	regfree(&preg);
	if (error < GIT_SUCCESS) {
		size_t i;
		char *elem;
		git_vector_foreach(&list, i, elem) {
			free(elem);
		}

		git_vector_free(&list);
		return error;
	}

	remotes_list->strings = (char **)list.contents;
	remotes_list->count = list.length;

	return GIT_SUCCESS;
}
