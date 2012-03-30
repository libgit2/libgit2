/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/remote.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/revwalk.h"

#include "common.h"
#include "transport.h"
#include "remote.h"
#include "refspec.h"
#include "pack.h"
#include "fetch.h"
#include "netops.h"

struct filter_payload {
	git_remote *remote;
	const git_refspec *spec;
	git_odb *odb;
	int found_head;
};

static int filter_ref__cb(git_remote_head *head, void *payload)
{
	struct filter_payload *p = payload;
	int ret;

	if (!p->found_head && strcmp(head->name, GIT_HEAD_FILE) == 0) {
		p->found_head = 1;
	} else {
		/* If it doesn't match the refpec, we don't want it */
		ret = git_refspec_src_match(p->spec, head->name);

		if (ret == GIT_ENOMATCH)
			return 0;

		if (ret < GIT_SUCCESS) {
			giterr_set(GITERR_NET, "Error matching remote ref name");
			return -1;
		}
	}

	/* If we have the object, mark it so we don't ask for it */
	if (git_odb_exists(p->odb, &head->oid))
		head->local = 1;
	else
		p->remote->need_pack = 1;

	return git_vector_insert(&p->remote->refs, head);
}

static int filter_wants(git_remote *remote)
{
	struct filter_payload p;

	git_vector_clear(&remote->refs);

	/*
	 * The fetch refspec can be NULL, and what this means is that the
	 * user didn't specify one. This is fine, as it means that we're
	 * not interested in any particular branch but just the remote's
	 * HEAD, which will be stored in FETCH_HEAD after the fetch.
	 */
	p.spec = git_remote_fetchspec(remote);
	p.found_head = 0;
	p.remote = remote;

	if (git_repository_odb__weakptr(&p.odb, remote->repo) < 0)
		return -1;

	return remote->transport->ls(remote->transport, &filter_ref__cb, &p);
}

/*
 * In this first version, we push all our refs in and start sending
 * them out. When we get an ACK we hide that commit and continue
 * traversing until we're done
 */
int git_fetch_negotiate(git_remote *remote)
{
	git_transport *t = remote->transport;

	if (filter_wants(remote) < 0) {
		giterr_set(GITERR_NET, "Failed to filter the reference list for wants");
		return -1;
	}

	/* Don't try to negotiate when we don't want anything */
	if (remote->refs.length == 0 || !remote->need_pack)
		return 0;

	/*
	 * Now we have everything set up so we can start tell the server
	 * what we want and what we have.
	 */
	return t->negotiate_fetch(t, remote->repo, &remote->refs);
}

int git_fetch_download_pack(char **out, git_remote *remote)
{
	if(!remote->need_pack) {
		*out = NULL;
		return 0;
	}

	return remote->transport->download_pack(out, remote->transport, remote->repo);
}

/* Receiving data from a socket and storing it is pretty much the same for git and HTTP */
int git_fetch__download_pack(
	char **out,
	const char *buffered,
	size_t buffered_size,
	GIT_SOCKET fd,
	git_repository *repo)
{
	git_filebuf file = GIT_FILEBUF_INIT;
	int error;
	char buff[1024];
	git_buf path = GIT_BUF_INIT;
	static const char suff[] = "/objects/pack/pack-received";
	gitno_buffer buf;

	gitno_buffer_setup(&buf, buff, sizeof(buff), fd);

	if (memcmp(buffered, "PACK", strlen("PACK"))) {
		giterr_set(GITERR_NET, "The pack doesn't start with the signature");
		return -1;
	}

	if (git_buf_joinpath(&path, repo->path_repository, suff) < 0)
		goto on_error;

	if (git_filebuf_open(&file, path.ptr, GIT_FILEBUF_TEMPORARY) < 0)
		goto on_error;

	/* Part of the packfile has been received, don't loose it */
	if (git_filebuf_write(&file, buffered, buffered_size) < 0)
		goto on_error;

	while (1) {
		if (git_filebuf_write(&file, buf.data, buf.offset) < 0)
			goto on_error;

		gitno_consume_n(&buf, buf.offset);
		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			goto on_error;
		if (error == 0) /* Orderly shutdown */
			break;
	}

	*out = git__strdup(file.path_lock);
	if (*out == NULL)
		goto on_error;

	/* A bit dodgy, but we need to keep the pack at the temporary path */
	if (git_filebuf_commit_at(&file, file.path_lock, GIT_PACK_FILE_MODE) < 0)
		goto on_error;

	git_buf_free(&path);

	return 0;
on_error:
	git_buf_free(&path);
	git_filebuf_cleanup(&file);
	return -1;
}
