/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "server.h"
#include "netops.h"

int git_server_new(git_server **out, int fd)
{
	git_server *server;

	server = git__calloc(1, sizeof(git_server));
	GITERR_CHECK_ALLOC(server);

	server->s.socket = fd;

	*out = server;
	return 0;
}

void git_server_free(git_server *server)
{
	if (server == NULL)
		return;

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

	return 0;
}
