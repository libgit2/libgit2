/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/revwalk.h"
#include "git2/transport.h"

#include "common.h"
#include "remote.h"
#include "refspec.h"
#include "pack.h"
#include "fetch.h"
#include "netops.h"

struct filter_payload {
	git_remote *remote;
	const git_refspec *spec, *tagspec;
	git_odb *odb;
	int found_head;
};

static int filter_ref__cb(git_remote_head *head, void *payload)
{
	struct filter_payload *p = payload;
	int match = 0;

	if (!git_reference_is_valid_name(head->name))
		return 0;

	if (!p->found_head && strcmp(head->name, GIT_HEAD_FILE) == 0)
		p->found_head = 1;
	else if (git_refspec_src_matches(p->spec, head->name))
			match = 1;
	else if (p->remote->download_tags == GIT_REMOTE_DOWNLOAD_TAGS_ALL &&
		 git_refspec_src_matches(p->tagspec, head->name))
			match = 1;

	if (!match)
		return 0;

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
	git_refspec tagspec;
	int error = -1;

	git_vector_clear(&remote->refs);
	if (git_refspec__parse(&tagspec, GIT_REFSPEC_TAGS, true) < 0)
		return error;

	/*
	 * The fetch refspec can be NULL, and what this means is that the
	 * user didn't specify one. This is fine, as it means that we're
	 * not interested in any particular branch but just the remote's
	 * HEAD, which will be stored in FETCH_HEAD after the fetch.
	 */
	p.spec = git_remote_fetchspec(remote);
	p.tagspec = &tagspec;
	p.found_head = 0;
	p.remote = remote;

	if (git_repository_odb__weakptr(&p.odb, remote->repo) < 0)
		goto cleanup;

	error = git_remote_ls(remote, filter_ref__cb, &p);

cleanup:
	git_refspec__free(&tagspec);

	return error;
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
	 * Now we have everything set up so we can start tell the
	 * server what we want and what we have.
	 */
	return t->negotiate_fetch(t,
		remote->repo,
		(const git_remote_head * const *)remote->refs.contents,
		remote->refs.length);
}

int git_fetch_download_pack(
	git_remote *remote,
	git_transfer_progress_callback progress_cb,
	void *progress_payload)
{
	git_transport *t = remote->transport;

	if(!remote->need_pack)
		return 0;

	return t->download_pack(t, remote->repo, &remote->stats, progress_cb, progress_payload);
}
