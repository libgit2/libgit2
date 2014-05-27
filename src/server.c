/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "server.h"
#include "netops.h"

int git_server_new(git_server **out, git_repository *repo, int fd)
{
	git_server *server;

	assert(out && repo);

	server = git__calloc(1, sizeof(git_server));
	GITERR_CHECK_ALLOC(server);

	server->repo = repo;
	server->s.socket = fd;

	*out = server;
	return 0;
}

void git_server_free(git_server *server)
{
	if (server == NULL)
		return;

	git_array_clear(server->wants);
	git_array_clear(server->common);
	git__free(server->path);
	git__free(server);
}

int git_server__handle_request(git_server *server, git_pkt *pkt)
{
	git_pkt_request *req;

	if (pkt->type != GIT_PKT_REQUEST) {
		giterr_set(GITERR_NET, "first line was not a request");
		return -1;
	}

	req = (git_pkt_request *) pkt;
	server->type = req->request;
	server->path = git__strdup(req->path);
	GITERR_CHECK_ALLOC(server->path);

	return 0;
}

int git_server__ls(git_buf *out, git_server *server)
{
	git_repository *repo = server->repo;
	git_strarray ref_names = {0};
	git_reference *ref = NULL;
	int error;
	size_t i;

	assert(out && server);

	if (server->type != GIT_REQUEST_UPLOAD_PACK) {
		giterr_set(GITERR_NET, "unsupported type");
	}

	if ((error = git_reference_list(&ref_names, repo)) < 0)
		return error;

	/* the references need to be alphasorted */
	git__tsort((void **)ref_names.strings, ref_names.count, git__strcmp_cb);

	if ((error = git_reference_lookup(&ref, repo, "HEAD")) < 0)
		goto cleanup;

	error = git_pkt_buffer_reference(out, ref);
	git_reference_free(ref);
	if (error < 0)
		return error;

	for (i = 0; i < ref_names.count; i++) {
		if ((error = git_reference_lookup(&ref, repo, ref_names.strings[i])) < 0)
			goto cleanup;

		error = git_pkt_buffer_reference(out, ref);
		git_reference_free(ref);
		if (error < 0)
			break;
	}

	if (error < 0)
		return -1;

	error = git_pkt_buffer_flush(out);

cleanup:
	git_strarray_free(&ref_names);
	return error;
}

int git_server__negotiation(git_server *server, git_pkt *_pkt)
{
	git_oid *id, *have_id;
	git_pkt_have_want *pkt;
	git_odb *odb = NULL;
	int error;

	if (_pkt->type != GIT_PKT_HAVE && _pkt->type != GIT_PKT_WANT) {
		giterr_set(GITERR_NET, "invalid pkt for negotiation");
		return -1;
	}

	pkt = (git_pkt_have_want *) _pkt;

	if (pkt->type == GIT_PKT_WANT) {
		id = git_array_alloc(server->wants);
		GITERR_CHECK_ALLOC(id);

		git_oid_cpy(id, &pkt->id);
		return 0;
	}

	/* we know it's a 'have', so we check to see if it's common */
	have_id = &pkt->id;
	if ((error = git_repository_odb(&odb, server->repo)) < 0)
		return error;

	if ((error = git_odb_exists(odb, have_id)) < 0)
		goto cleanup;

	if (error == 1) {
		error = 0;
		id = git_array_alloc(server->common);
		GITERR_CHECK_ALLOC(id);

		git_oid_cpy(id, &pkt->id);
	}

cleanup:
	git_odb_free(odb);

	return error;
}

int git_server_run(git_server *server)
{
	/* 65535 is the max size of a pkt frame */
	char buffer[65536] = {0};
	gitno_buffer buf;
	gitno_socket *s = &server->s;
	int error;

	gitno_buffer_setup(s, &buf, buffer, sizeof(buffer));

	/* first we figure out which service the user wants */
	while (1) {
		git_pkt *pkt;
		const char *rest;


		if ((error = gitno_recv(&buf)) < 0)
			return error;

		error = git_pkt_parse_line(&pkt, buffer, &rest, buf.len);
		if (error == GIT_EBUFS)
			continue;

		if (error < 0)
			return error;

		gitno_consume(&buf, rest);

		error = git_server__handle_request(server, pkt);
		git_pkt_free(pkt);

		if (error < 0)
			return error;

		break;
	}

	/* and now we let the server respond with the listing */

	return 0;
}
