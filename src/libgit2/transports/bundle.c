/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

/*
 * Bundle transport: implements git_transport over a .bundle file.
 *
 * Supports fetch/clone; push is not supported.
 *
 * Design notes
 * ------------
 * - The bundle is opened (header parsed) in connect().
 * - The file descriptor is kept open across connect() → download_pack() to
 *   detect TOCTOU file replacement.  A fstat() before streaming confirms
 *   that the file has not changed size since open.
 * - download_pack() seeks to pack_start_offset and streams in CHUNK_SIZE
 *   chunks, checking cancellation between each chunk.
 * - The ODB writepack follows the strict commit/abort protocol: commit() is
 *   called only on full success; free() is called on all paths.
 */

#include "common.h"

#include "bundle.h"
#include "fs_path.h"
#include "posix.h"
#include "remote.h"
#include "repository.h"

#include "git2/bundle.h"
#include "git2/net.h"
#include "git2/odb.h"
#include "git2/odb_backend.h"
#include "git2/remote.h"
#include "git2/repository.h"
#include "git2/revparse.h"
#include "git2/sys/remote.h"
#include "git2/sys/transport.h"
#include "git2/transport.h"
#include "git2/types.h"

#define BUNDLE_CHUNK_SIZE (256 * 1024) /* 256 KiB read chunks */

typedef struct {
	git_transport parent;
	git_remote *owner;
	char *url;
	int direction;
	git_atomic32 cancelled;
	git_bundle *bundle;          /* NULL until connected */
	git_file pack_fd;            /* kept open for TOCTOU safety; -1 if closed */
	uint64_t pack_file_size;     /* file size recorded at connect() */
	git_remote_connect_options connect_opts;
	unsigned connected : 1;
} transport_bundle;

/* ---------------------------------------------------------------------- */

static int bundle_connect(
	git_transport *transport,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_str path = GIT_STR_INIT;
	struct stat st;
	int error;

	if (t->connected)
		return 0;

	if (direction == GIT_DIRECTION_PUSH) {
		git_error_set(GIT_ERROR_NET,
			"bundle transport does not support push");
		return GIT_ENOTSUPPORTED;
	}

	if (git_remote_connect_options_normalize(
	        &t->connect_opts, t->owner->repo, connect_opts) < 0)
		return -1;

	/* convert bundle:// URL or plain path to a filesystem path */
	if (git__prefixcmp(url, "bundle://") == 0) {
		if ((error = git_str_sets(&path, url + strlen("bundle://"))) < 0)
			goto done;
	} else {
		if ((error = git_fs_path_from_url_or_path(&path, url)) < 0)
			goto done;
	}

	t->url = git__strdup(url);
	GIT_ERROR_CHECK_ALLOC(t->url);

	if ((error = git_bundle_open(&t->bundle, path.ptr)) < 0)
		goto done;

	/*
	 * Open the file descriptor here and keep it open.  This lets us
	 * detect TOCTOU replacement in download_pack().
	 */
	t->pack_fd = p_open(path.ptr, O_RDONLY, 0);
	if (t->pack_fd < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: cannot open '%s'", path.ptr);
		error = -1;
		goto done;
	}

	if (p_fstat(t->pack_fd, &st) < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: fstat failed on '%s'", path.ptr);
		error = -1;
		goto done;
	}
	t->pack_file_size = (uint64_t)st.st_size;

	t->direction = direction;
	t->connected = 1;

done:
	git_str_dispose(&path);
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

	return git_remote_connect_options_normalize(
		&t->connect_opts, t->owner->repo, connect_opts);
}

static int bundle_capabilities(
	unsigned int *capabilities,
	git_transport *transport)
{
	GIT_UNUSED(transport);

	/* We can report exact tip OIDs; we cannot report arbitrary reachable OIDs */
	*capabilities = GIT_REMOTE_CAPABILITY_TIP_OID;
	return 0;
}

#ifdef GIT_EXPERIMENTAL_SHA256
static int bundle_oid_type(git_oid_t *out, git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	GIT_ASSERT(t->bundle);
	*out = t->bundle->oid_type;
	return 0;
}
#endif

static int bundle_ls(
	const git_remote_head ***out,
	size_t *size,
	git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	/*
	 * The bundle is parsed during connect() and kept alive through
	 * close() so that checkout_branch() can call git_remote_ls()
	 * after the fetch phase has closed the transport.
	 */
	if (!t->bundle) {
		git_error_set(GIT_ERROR_NET,
			"bundle: refs requested before connect");
		return -1;
	}

	return git_bundle_refs(out, size, t->bundle);
}

static int bundle_negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_remote_head *rhead;
	unsigned int i;
	int error;

	if (wants->depth) {
		git_error_set(GIT_ERROR_NET,
			"shallow fetch is not supported by the bundle transport");
		return GIT_ENOTSUPPORTED;
	}

	/* Verify prerequisites */
	if ((error = git_bundle_verify(repo, t->bundle)) < 0)
		return error;

	/* Fill in loids (what the local repo already has at each ref) */
	git_vector_foreach(&t->bundle->refs, i, rhead) {
		git_object *obj;

		error = git_revparse_single(&obj, repo, rhead->name);
		if (!error) {
			git_oid_cpy(&rhead->loid, git_object_id(obj));
			rhead->local = 1;
		} else if (error == GIT_ENOTFOUND) {
			git_error_clear();
		} else {
			return error;
		}
		git_object_free(obj);
	}

	return 0;
}

static int bundle_shallow_roots(
	git_oidarray *out,
	git_transport *transport)
{
	GIT_UNUSED(out);
	GIT_UNUSED(transport);
	/* Bundles do not carry shallow information */
	return 0;
}

static int bundle_download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	transport_bundle *t = (transport_bundle *)transport;
	git_odb *odb = NULL;
	git_odb_writepack *writepack = NULL;
	unsigned char *chunk = NULL;
	struct stat st;
	uint64_t pack_bytes;
	uint64_t offset;
	int error = 0;

	if (!t->bundle || t->pack_fd < 0) {
		git_error_set(GIT_ERROR_NET,
			"bundle: download attempted when transport is not open");
		return -1;
	}

	/* TOCTOU check: ensure the file has not changed since connect() */
	if (p_fstat(t->pack_fd, &st) < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: fstat failed during download");
		return -1;
	}

	if ((uint64_t)st.st_size != t->pack_file_size) {
		git_error_set(GIT_ERROR_NET,
			"bundle: file changed between connect and download");
		return -1;
	}

	if (t->bundle->pack_start_offset > (uint64_t)st.st_size) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: pack offset exceeds file size");
		return GIT_EINVALID;
	}

	pack_bytes = (uint64_t)st.st_size - t->bundle->pack_start_offset;

	/* Seek to pack start */
	if (p_lseek(t->pack_fd, (off_t)t->bundle->pack_start_offset,
	            SEEK_SET) < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: seek to packfile failed");
		return -1;
	}

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0)
		return error;

	if ((error = git_odb_write_pack(
	         &writepack, odb,
	         t->connect_opts.callbacks.transfer_progress,
	         t->connect_opts.callbacks.payload)) < 0)
		return error;

	stats->total_objects    = 0;
	stats->indexed_objects  = 0;
	stats->received_objects = 0;
	stats->received_bytes   = 0;

	chunk = git__malloc(BUNDLE_CHUNK_SIZE);
	if (!chunk) {
		error = -1;
		goto cleanup;
	}

	offset = 0;
	while (offset < pack_bytes) {
		size_t to_read = BUNDLE_CHUNK_SIZE;
		ssize_t nread;

		if (git_atomic32_get(&t->cancelled)) {
			git_error_set(GIT_ERROR_NET, "bundle: download cancelled");
			error = GIT_EUSER;
			goto cleanup;
		}

		if ((uint64_t)to_read > pack_bytes - offset)
			to_read = (size_t)(pack_bytes - offset);

		nread = p_read(t->pack_fd, chunk, to_read);
		if (nread < 0) {
			git_error_set(GIT_ERROR_OS,
				"bundle: read error during pack streaming");
			error = -1;
			goto cleanup;
		}
		if (nread == 0)
			break; /* EOF */

		stats->received_bytes += (size_t)nread;

		if ((error = writepack->append(
		         writepack, chunk, (size_t)nread, stats)) < 0)
			goto cleanup;

		offset += (size_t)nread;
	}

	error = writepack->commit(writepack, stats);

cleanup:
	git__free(chunk);
	if (writepack)
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

static int bundle_push(
	git_transport *transport,
	git_push *push)
{
	GIT_UNUSED(transport);
	GIT_UNUSED(push);

	git_error_set(GIT_ERROR_NET,
		"bundle transport does not support push");
	return GIT_ENOTSUPPORTED;
}

static int bundle_close(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	t->connected = 0;

	/*
	 * Close the pack file descriptor but keep t->bundle alive: the refs
	 * it holds are needed by checkout_branch() which calls git_remote_ls()
	 * after the fetch phase has already closed the transport.  The bundle
	 * is freed only in bundle_free().
	 */
	if (t->pack_fd >= 0) {
		p_close(t->pack_fd);
		t->pack_fd = -1;
	}

	git__free(t->url);
	t->url = NULL;

	return 0;
}

static void bundle_free(git_transport *transport)
{
	transport_bundle *t = (transport_bundle *)transport;

	bundle_close(transport);

	if (t->bundle) {
		git_bundle_free(t->bundle);
		t->bundle = NULL;
	}

	git_remote_connect_options_dispose(&t->connect_opts);
	git__free(t);
}

/* -------------------------------------------------------------------------
 * Public factory
 * ---------------------------------------------------------------------- */

int git_transport_bundle(
	git_transport **out,
	git_remote *owner,
	void *param)
{
	transport_bundle *t;

	GIT_UNUSED(param);

	t = git__calloc(1, sizeof(transport_bundle));
	GIT_ERROR_CHECK_ALLOC(t);

	t->parent.version      = GIT_TRANSPORT_VERSION;
	t->parent.connect      = bundle_connect;
	t->parent.set_connect_opts = bundle_set_connect_opts;
	t->parent.capabilities = bundle_capabilities;
#ifdef GIT_EXPERIMENTAL_SHA256
	t->parent.oid_type     = bundle_oid_type;
#endif
	t->parent.ls           = bundle_ls;
	t->parent.negotiate_fetch = bundle_negotiate_fetch;
	t->parent.shallow_roots   = bundle_shallow_roots;
	t->parent.download_pack   = bundle_download_pack;
	t->parent.push         = bundle_push;
	t->parent.is_connected = bundle_is_connected;
	t->parent.cancel       = bundle_cancel;
	t->parent.close        = bundle_close;
	t->parent.free         = bundle_free;

	t->pack_fd = -1;
	t->owner   = owner;

	*out = (git_transport *)t;
	return 0;
}
