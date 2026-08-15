/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "bundle.h"
#include "fs_path.h"
#include "net.h"
#include "odb.h"
#include "posix.h"
#include "refs.h"
#include "remote.h"
#include "repository.h"

#include "git2/odb.h"
#include "git2/sys/transport.h"

#define BUNDLE_PACK_CHUNK 65536

typedef struct {
	git_transport parent;
	git_remote *owner;
	int fd;
	git_bundle_parser parser;
	git_remote_connect_options connect_opts;
	git_atomic32 cancelled;
	unsigned connected : 1,
		have_refs : 1,
		verified : 1;
} transport_bundle;

/*
 * A local path, as opposed to an scp-style `host:path` remote.  A
 * Windows drive-rooted path is a local path even though it contains a
 * colon; anything else whose colon precedes the first path separator
 * is scp-style and is left to the ssh transport.
 */
static bool bundle_is_local_path(const char *url)
{
	const char *c;

	if (git_net_str_is_url(url))
		return false;

#ifdef GIT_WIN32
	if (git_fs_path_root(url) > 0)
		return true;
#endif

	for (c = url; *c; c++) {
		if (*c == ':')
			return false;

		if (git_fs_path_is_dirsep(*c))
			return true;
	}

	return true;
}

int git_transport_bundle__probe(bool *out, const char *url)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	struct stat st;
	int fd, error;

	*out = false;

	if (!bundle_is_local_path(url))
		return 0;

	/*
	 * Only regular files can be bundles.  Avoid opening special files,
	 * which may block or have side effects.
	 */
	if (p_stat(url, &st) < 0 || !S_ISREG(st.st_mode))
		return 0;

	if ((fd = p_open(url, O_RDONLY)) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to open '%s'", url);
		return -1;
	}

	error = git_bundle_parser_parse_fd(&parser, fd);
	git_bundle_parser_dispose(&parser);
	p_close(fd);

	if (error == 0) {
		*out = true;
		return 0;
	}

	if (error == GIT_ENOTSUPPORTED) {
		/* connect will report the specific reason */
		git_error_clear();
		*out = true;
		return 0;
	}

	if (error == GIT_EINVALID) {
		git_error_clear();
		return 0;
	}

	return error;
}

/*
 * Advertise a recorded HEAD first, the way the smart transports do,
 * without disturbing the order of the other references.
 */
static void move_head_first(git_vector *refs)
{
	git_remote_head *head;
	size_t i;

	git_vector_foreach(refs, i, head) {
		if (strcmp(head->name, GIT_HEAD_REF) != 0)
			continue;

		while (i > 0) {
			refs->contents[i] = refs->contents[i - 1];
			i--;
		}

		refs->contents[0] = head;
		return;
	}
}

static void bundle_reset(transport_bundle *t)
{
	if (t->fd >= 0) {
		p_close(t->fd);
		t->fd = -1;
	}

	git_bundle_parser_dispose(&t->parser);

	t->connected = 0;
	t->have_refs = 0;
	t->verified = 0;

	git_atomic32_set(&t->cancelled, 0);
}

static int bundle_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	transport_bundle *t = (transport_bundle *)transport;
	int error;

	if (t->connected)
		return 0;

	if (direction != GIT_DIRECTION_FETCH) {
		git_error_set(GIT_ERROR_NET,
			"the bundle transport does not support pushing");
		return GIT_ENOTSUPPORTED;
	}

	if (git_remote_connect_options_normalize(&t->connect_opts,
			t->owner->repo, connect_opts) < 0)
		return -1;

	bundle_reset(t);

	if ((t->fd = p_open(url, O_RDONLY)) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to open bundle '%s'", url);
		error = -1;
		goto on_error;
	}

	if ((error = git_bundle_parser_parse_fd(&t->parser, t->fd)) < 0)
		goto on_error;

	/* the descriptor is now positioned at the first byte of the pack */

	move_head_first(&t->parser.header.refs);

	t->connected = 1;
	t->have_refs = 1;

	return 0;

on_error:
	bundle_reset(t);
	return error;
}

static int bundle_set_connect_opts(
	git_transport *transport,
	const git_remote_connect_options *connect_opts)
{
	transport_bundle *t = (transport_bundle *)transport;

	if (!t->connected) {
		git_error_set(GIT_ERROR_NET,
			"cannot reconfigure a transport that is not connected");
		return -1;
	}

	return git_remote_connect_options_normalize(&t->connect_opts,
		t->owner->repo, connect_opts);
}

static int bundle_capabilities(unsigned int *capabilities, git_transport *transport)
{
	GIT_UNUSED(transport);

	/*
	 * A bundle can serve its advertised tips, but it cannot promise an
	 * arbitrary reachable object that was not packed into it.
	 */
	*capabilities = GIT_REMOTE_CAPABILITY_TIP_OID;

	return 0;
}

static int bundle_oid_type(git_oid_t *out, git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	*out = t->parser.header.oid_type;

	return 0;
}

static int bundle_ls(
	const git_remote_head ***out, size_t *size, git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	if (!t->have_refs) {
		git_error_set(GIT_ERROR_NET,
			"the transport has not yet loaded the refs");
		return -1;
	}

	*out = (const git_remote_head **)t->parser.header.refs.contents;
	*size = t->parser.header.refs.length;

	return 0;
}

static int bundle_push(git_transport *transport, git_push *push)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(push);

	git_error_set(GIT_ERROR_NET,
		"the bundle transport does not support pushing");
	return GIT_ENOTSUPPORTED;
}

static int bundle_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_oid_t repo_oid_type;
	int error;

	/* a result must never leak across attempts or repositories */
	t->verified = 0;

	if (git_atomic32_get(&t->cancelled)) {
		git_atomic32_set(&t->cancelled, 0);
		git_error_set(GIT_ERROR_NET, "the fetch was cancelled");
		return GIT_EUSER;
	}

	if (wants->depth != GIT_FETCH_DEPTH_FULL) {
		git_error_set(GIT_ERROR_NET,
			"shallow fetch is not supported by the bundle transport");
		return GIT_ENOTSUPPORTED;
	}

	/*
	 * A successful download writes the remote's (empty) shallow root
	 * list, which would delete the destination's `shallow` file.
	 */
	if (wants->shallow_roots_len > 0) {
		git_error_set(GIT_ERROR_NET,
			"cannot fetch a bundle into a shallow repository");
		return GIT_ENOTSUPPORTED;
	}

	repo_oid_type = git_repository_oid_type(repo);

	if (repo_oid_type != t->parser.header.oid_type) {
		git_error_set(GIT_ERROR_NET,
			"the bundle uses %s object ids, but the repository uses %s",
			git_oid_type_name(t->parser.header.oid_type),
			git_oid_type_name(repo_oid_type));
		return -1;
	}

	if ((error = git_bundle_check_prerequisites(&t->parser.header, repo)) < 0)
		return error;

	t->verified = 1;

	return 0;
}

static int bundle_shallow_roots(git_oidarray *out, git_transport *transport)
{
	GIT_UNUSED(out);
	GIT_UNUSED(transport);

	/* negotiation established that neither side is shallow */
	return 0;
}

static int bundle_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_odb_writepack *writepack = NULL;
	git_odb *odb;
	char buf[BUNDLE_PACK_CHUNK];
	ssize_t read_len;
	int error;

	if (!t->connected || t->fd < 0) {
		git_error_set(GIT_ERROR_NET, "the bundle is not open");
		return -1;
	}

	if (!t->verified) {
		git_error_set(GIT_ERROR_NET,
			"the bundle has not been verified against this repository");
		return -1;
	}

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;

	/*
	 * Connect left the descriptor here, but a previous download may
	 * have moved it; a retry must start at the first byte of the pack.
	 */
	if (p_lseek(t->fd, (off_t)t->parser.header.pack_offset, SEEK_SET) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to seek in bundle");
		return -1;
	}

	/*
	 * The write-pack resolves thin-pack bases against this repository,
	 * so an incremental bundle's deltas find their prerequisites here.
	 */
	if ((error = git_odb_write_pack(&writepack, odb,
			t->connect_opts.callbacks.transfer_progress,
			t->connect_opts.callbacks.payload)) < 0)
		return error;

	memset(stats, 0, sizeof(*stats));

	while (true) {
		if (git_atomic32_get(&t->cancelled)) {
			git_atomic32_set(&t->cancelled, 0);
			git_error_set(GIT_ERROR_NET, "the fetch was cancelled");
			error = GIT_EUSER;
			goto done;
		}

		if ((read_len = p_read(t->fd, buf, sizeof(buf))) < 0) {
			git_error_set(GIT_ERROR_OS, "failed to read bundle pack");
			error = -1;
			goto done;
		}

		if (read_len == 0)
			break;

		stats->received_bytes += (size_t)read_len;

		if ((error = writepack->append(writepack, buf,
				(size_t)read_len, stats)) < 0)
			goto done;
	}

	/* only a clean end of the bundle may publish the pack */
	error = writepack->commit(writepack, stats);

done:
	writepack->free(writepack);
	return error;
}

static int bundle_is_connected(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	return t->connected;
}

static void bundle_cancel(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	git_atomic32_set(&t->cancelled, 1);
}

static int bundle_close(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	/*
	 * Close the descriptor but keep the advertised references, which
	 * callers may still read after disconnecting.
	 */
	if (t->fd >= 0) {
		p_close(t->fd);
		t->fd = -1;
	}

	t->connected = 0;
	t->verified = 0;

	git_atomic32_set(&t->cancelled, 0);

	return 0;
}

static void bundle_free(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	bundle_reset(t);
	git_remote_connect_options_dispose(&t->connect_opts);
	git__free(t);
}

int git_transport_bundle(git_transport **out, git_remote *owner, void *param)
{
	transport_bundle *t;

	GIT_UNUSED(param);

	t = git__calloc(1, sizeof(transport_bundle));
	GIT_ERROR_CHECK_ALLOC(t);

	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.connect = bundle_connect;
	t->parent.set_connect_opts = bundle_set_connect_opts;
	t->parent.capabilities = bundle_capabilities;
	t->parent.oid_type = bundle_oid_type;
	t->parent.ls = bundle_ls;
	t->parent.push = bundle_push;
	t->parent.negotiate_fetch = bundle_negotiate_fetch;
	t->parent.shallow_roots = bundle_shallow_roots;
	t->parent.download_pack = bundle_download_pack;
	t->parent.is_connected = bundle_is_connected;
	t->parent.cancel = bundle_cancel;
	t->parent.close = bundle_close;
	t->parent.free = bundle_free;

	t->owner = owner;
	t->fd = -1;
	t->parser.header.oid_type = GIT_OID_SHA1;

	*out = (git_transport *)t;

	return 0;
}
