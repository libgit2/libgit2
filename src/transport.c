/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "common.h"
#include "git2/types.h"
#include "git2/remote.h"
#include "git2/net.h"
#include "git2/transport.h"
#include "git2/sys/transport.h"
#include "path.h"

typedef struct transport_definition {
	char *scheme;
	git_transport_query_cb query_fn;
	git_transport_init_cb init_fn;
	void *param;
} transport_definition;

static git_smart_subtransport_definition http_subtransport_definition = { git_smart_subtransport_http, 1 };
static git_smart_subtransport_definition git_subtransport_definition = { git_smart_subtransport_git, 0 };
#ifdef GIT_SSH
static git_smart_subtransport_definition ssh_subtransport_definition = { git_smart_subtransport_ssh, 0 };
#endif

static transport_definition transports[] = {
	{ "git",   NULL, git_transport_smart, &git_subtransport_definition },
	{ "http",  NULL, git_transport_smart, &http_subtransport_definition },
	{ "https", NULL, git_transport_smart, &http_subtransport_definition },
	{ "file",  NULL, git_transport_local, NULL },
#ifdef GIT_SSH
	{ "ssh",   NULL, git_transport_smart, &ssh_subtransport_definition },
#else
	{ "ssh",   NULL, git_transport_dummy, NULL },
#endif
};

static int transport_definition_cmp(const void *one, const void *two)
{
	const transport_definition *def1 = one, *def2 = two;
	int def1_wc = (strcmp(def1->scheme, "*") == 0);
	int def2_wc = (strcmp(def2->scheme, "*") == 0);

	if (def1_wc && def2_wc)
		return 0;
	else if (def1_wc)
		return 1;
	else if (def2_wc)
		return -1;

	return strcmp(def1->scheme, def2->scheme);
}

static git_vector custom_transports = { 0, transport_definition_cmp };

static int transport_for_scheme(
	transport_definition **out,
	const char *scheme,
	const char *url)
{
	transport_definition *d;
	size_t i = 0;
	int error = 0;

	/* See if we have a custom transport for this scheme (or all schemes) */
	git_vector_foreach(&custom_transports, i, d) {
		if (strcasecmp(d->scheme, scheme) == 0 ||
			strcmp(d->scheme, "*") == 0) {

			unsigned int accepted = 1;

			if (d->query_fn && 
				(error = d->query_fn(&accepted, scheme, url, d->param)) < 0)
				return error;

			if (accepted) {
				*out = d;
				return 0;
			}
		}
	}

	/* See if there's a system transport for this scheme */
	for (i = 0; i < ARRAY_SIZE(transports); i++) {
		if (strcmp(transports[i].scheme, scheme) == 0) {
			*out = &transports[i];
			return 0;
		}
	}

	return 0;
}

static int scheme_for_url(git_buf *scheme, const char *url)
{
	const char *end;

	if ((end = strstr(url, "://")) == NULL)
		return 0;

	return git_buf_put(scheme, url, (end - url));
}

static int transport_find(
	transport_definition **out,
	git_buf *scheme,
	const char *url)
{
	transport_definition *def = NULL;
	int error = 0;

	if ((error = scheme_for_url(scheme, url)) < 0 ||
		(error = transport_for_scheme(&def, scheme->ptr, url)) < 0)
		return error;

#ifdef GIT_WIN32
	/* On Windows, it might not be possible to discern between absolute local
	 * and ssh paths - first check if this is a valid local path that points
	 * to a directory and if so assume local path, else assume SSH */
	if (!def && git_path_exists(url) && git_path_isdir(url)) {
		if ((error = git_buf_puts(scheme, "file")) < 0 ||
			(error = transport_for_scheme(&def, scheme->ptr, url)) < 0)
			return error;
	}
#endif

	/* It could be a SSH remote path. Check to see if there's a :
	 * On non-Windows, we do this before going to the filesystem. */
	if (!def && strrchr(url, ':')) {
		if ((error = git_buf_puts(scheme, "ssh")) < 0 ||
			(error = transport_for_scheme(&def, scheme->ptr, url)) < 0)
			return error;
	}

#ifndef GIT_WIN32
	/* Check to see if the path points to a file on the local file system */
	if (!def && git_path_exists(url) && git_path_isdir(url)) {
		if ((error = git_buf_puts(scheme, "file")) < 0 ||
			(error = transport_for_scheme(&def, scheme->ptr, url)) < 0)
			return error;
	}
#endif

	*out = def;
	return def ? 0 : GIT_ENOTFOUND;
}

/**************
 * Public API *
 **************/

int git_transport_dummy(
	git_transport **transport,
	const char *scheme,
	const char *url,
	git_remote *owner,
	void *param)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(scheme);
	GIT_UNUSED(url);
	GIT_UNUSED(owner);
	GIT_UNUSED(param);

	giterr_set(GITERR_NET, "This transport isn't implemented. Sorry");
	return -1;
}

int git_transport_new(git_transport **out, git_remote *owner, const char *url)
{
	transport_definition *definition;
	git_transport *transport;
	git_buf scheme = GIT_BUF_INIT;
	int error;

	if ((error = transport_find(&definition, &scheme, url)) == GIT_ENOTFOUND) {
		giterr_set(GITERR_NET, "Unsupported URL protocol");
		goto done;
	} else if (error < 0)
		goto done;

	error = definition->init_fn(
		&transport, scheme.ptr, url, owner, definition->param);

	git_buf_free(&scheme);

	if (error >= 0)
		*out = transport;

done:
	git_buf_free(&scheme);
	return error;
}

int git_transport_register(
	const char *scheme,
	git_transport_query_cb query_cb,
	git_transport_init_cb init_cb,
	void *param)
{
	transport_definition *d, *definition = NULL;
	size_t i;
	int error = 0;

	assert(scheme);
	assert(init_cb);

	git_vector_foreach(&custom_transports, i, d) {
		if (strcasecmp(d->scheme, scheme) == 0) {
			error = GIT_EEXISTS;
			goto on_error;
		}
	}

	definition = git__calloc(1, sizeof(transport_definition));
	GITERR_CHECK_ALLOC(definition);

	definition->scheme = git__strdup(scheme);
	GITERR_CHECK_ALLOC(definition->scheme);

	definition->query_fn = query_cb;
	definition->init_fn = init_cb;
	definition->param = param;

	if (git_vector_insert_sorted(&custom_transports, definition, NULL) < 0)
		goto on_error;

	return 0;

on_error:
	if (definition) {
		git__free(definition->scheme);
		git__free(definition);
	}

	return error;
}

int git_transport_unregister(const char *scheme)
{
	transport_definition *d;
	size_t i;
	int error = 0;

	assert(scheme);

	git_vector_foreach(&custom_transports, i, d) {
		if (strcasecmp(d->scheme, scheme) == 0) {
			if ((error = git_vector_remove(&custom_transports, i)) < 0)
				goto done;

			git__free(d->scheme);
			git__free(d);

			if (!custom_transports.length)
				git_vector_free(&custom_transports);

			error = 0;
			goto done;
		}
	}

	error = GIT_ENOTFOUND;

done:
	return error;
}

/* from remote.h */
static transport_definition *transport_for_url(const char *url)
{
	transport_definition *definition = NULL;
	git_buf scheme = GIT_BUF_INIT;

	if (transport_find(&definition, &scheme, url) < 0)
		giterr_clear();

	git_buf_free(&scheme);
	return definition;
}

int git_remote_valid_url(const char *url)
{
	return (transport_for_url(url) != NULL);
}

int git_remote_supported_url(const char *url)
{
	transport_definition *def;

	return ((def = transport_for_url(url)) != NULL && def->init_fn != &git_transport_dummy);
}

int git_transport_init(git_transport *opts, int version)
{
	if (version != GIT_TRANSPORT_VERSION) {
		giterr_set(GITERR_INVALID, "Invalid version %d for git_transport", version);
		return -1;
	} else {
		git_transport o = GIT_TRANSPORT_INIT;
		memcpy(opts, &o, sizeof(o));
		return 0;
	}
}
