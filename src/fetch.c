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
#include "git2/indexer.h"

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

	if (!p->found_head && strcmp(head->name, GIT_HEAD_FILE) == 0) {
		p->found_head = 1;
	} else {
		/* If it doesn't match the refpec, we don't want it */
		if (!git_refspec_src_matches(p->spec, head->name))
			return 0;

		/* Don't even try to ask for the annotation target */
		if (!git__suffixcmp(head->name, "^{}"))
			return 0;
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

int git_fetch_download_pack(git_remote *remote, git_off_t *bytes, git_indexer_stats *stats)
{
	if(!remote->need_pack)
		return 0;

	return remote->transport->download_pack(remote->transport, remote->repo, bytes, stats);
}

/* Receiving data from a socket and storing it is pretty much the same for git and HTTP */
int git_fetch__download_pack(
	const char *buffered,
	size_t buffered_size,
	GIT_SOCKET fd,
	git_repository *repo,
	git_off_t *bytes,
	git_indexer_stats *stats)
{
	int recvd;
	char buff[1024];
	gitno_buffer buf;
	git_indexer_stream *idx;

	gitno_buffer_setup(&buf, buff, sizeof(buff), fd);

	if (memcmp(buffered, "PACK", strlen("PACK"))) {
		giterr_set(GITERR_NET, "The pack doesn't start with the signature");
		return -1;
	}

	if (git_indexer_stream_new(&idx, git_repository_path(repo)) < 0)
		return -1;

	memset(stats, 0, sizeof(git_indexer_stats));
	if (git_indexer_stream_add(idx, buffered, buffered_size, stats) < 0)
		goto on_error;

	*bytes = buffered_size;

	do {
		if (git_indexer_stream_add(idx, buf.data, buf.offset, stats) < 0)
			goto on_error;

		gitno_consume_n(&buf, buf.offset);
		if ((recvd = gitno_recv(&buf)) < 0)
			goto on_error;

		*bytes += recvd;
	} while(recvd > 0);

	if (git_indexer_stream_finalize(idx, stats))
		goto on_error;

	git_indexer_stream_free(idx);
	return 0;

on_error:
	git_indexer_stream_free(idx);
	return -1;
}

int git_fetch_setup_walk(git_revwalk **out, git_repository *repo)
{
	git_revwalk *walk;
	git_strarray refs;
	unsigned int i;
	git_reference *ref;

	if (git_reference_list(&refs, repo, GIT_REF_LISTALL) < 0)
		return -1;

	if (git_revwalk_new(&walk, repo) < 0)
		return -1;

	git_revwalk_sorting(walk, GIT_SORT_TIME);

	for (i = 0; i < refs.count; ++i) {
		/* No tags */
		if (!git__prefixcmp(refs.strings[i], GIT_REFS_TAGS_DIR))
			continue;

		if (git_reference_lookup(&ref, repo, refs.strings[i]) < 0)
			goto on_error;

		if (git_reference_type(ref) == GIT_REF_SYMBOLIC)
			continue;
		if (git_revwalk_push(walk, git_reference_oid(ref)) < 0)
			goto on_error;

		git_reference_free(ref);
	}

	git_strarray_free(&refs);
	*out = walk;
	return 0;

on_error:
	git_reference_free(ref);
	git_strarray_free(&refs);
	return -1;
}
