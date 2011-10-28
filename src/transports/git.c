/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/net.h"
#include "git2/common.h"
#include "git2/types.h"
#include "git2/errors.h"
#include "git2/net.h"
#include "git2/revwalk.h"

#include "vector.h"
#include "transport.h"
#include "pkt.h"
#include "common.h"
#include "netops.h"
#include "filebuf.h"
#include "repository.h"
#include "fetch.h"

typedef struct {
	git_transport parent;
	GIT_SOCKET socket;
	git_vector refs;
	git_remote_head **heads;
	git_transport_caps caps;
	char buff[1024];
	gitno_buffer buf;
#ifdef GIT_WIN32
	WSADATA wsd;
#endif
} transport_git;

/*
 * Create a git procol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 */
static int gen_proto(git_buf *request, const char *cmd, const char *url)
{
	char *delim, *repo;
	char default_command[] = "git-upload-pack";
	char host[] = "host=";
	int len;

	delim = strchr(url, '/');
	if (delim == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to create proto-request: malformed URL");

	repo = delim;

	delim = strchr(url, ':');
	if (delim == NULL)
		delim = strchr(url, '/');

	if (cmd == NULL)
		cmd = default_command;

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + strlen(host) + (delim - url) + 1;

	git_buf_grow(request, len);
	git_buf_printf(request, "%04x%s %s%c%s", len, cmd, repo, 0, host);
	git_buf_put(request, url, delim - url);
	git_buf_putc(request, '\0');

	return git_buf_oom(request);
}

static int send_request(GIT_SOCKET s, const char *cmd, const char *url)
{
	int error;
	git_buf request = GIT_BUF_INIT;

	error = gen_proto(&request, cmd, url);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = gitno_send(s, request.ptr, request.size, 0);

cleanup:
	git_buf_free(&request);
	return error;
}

/*
 * Parse the URL and connect to a server, storing the socket in
 * out. For convenience this also takes care of asking for the remote
 * refs
 */
static int do_connect(transport_git *t, const char *url)
{
	GIT_SOCKET s;
	char *host, *port;
	const char prefix[] = "git://";
	int error, connected = 0;

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	error = gitno_extract_host_and_port(&host, &port, url, GIT_DEFAULT_PORT);
	if (error < GIT_SUCCESS)
		return error;

	s = gitno_connect(host, port);
	connected = 1;
	error = send_request(s, NULL, url);
	t->socket = s;

	git__free(host);
	git__free(port);

	if (error < GIT_SUCCESS && s > 0)
		close(s);
	if (!connected)
		error = git__throw(GIT_EOSERR, "Failed to connect to any of the addresses");

	return error;
}

/*
 * Read from the socket and store the references in the vector
 */
static int store_refs(transport_git *t)
{
	gitno_buffer *buf = &t->buf;
	git_vector *refs = &t->refs;
	int error = GIT_SUCCESS;
	const char *line_end, *ptr;
	git_pkt *pkt;


	while (1) {
		error = gitno_recv(buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(GIT_EOSERR, "Failed to receive data");
		if (error == GIT_SUCCESS) /* Orderly shutdown, so exit */
			return GIT_SUCCESS;

		ptr = buf->data;
		while (1) {
			if (buf->offset == 0)
				break;
			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
			/*
			 * If the error is GIT_ESHORTBUFFER, it means the buffer
			 * isn't long enough to satisfy the request. Break out and
			 * wait for more input.
			 * On any other error, fail.
			 */
			if (error == GIT_ESHORTBUFFER) {
				break;
			}
			if (error < GIT_SUCCESS) {
				return error;
			}

			/* Get rid of the part we've used already */
			gitno_consume(buf, line_end);

			error = git_vector_insert(refs, pkt);
			if (error < GIT_SUCCESS)
				return error;

			if (pkt->type == GIT_PKT_FLUSH)
				return GIT_SUCCESS;

		}
	}

	return error;
}

static int detect_caps(transport_git *t)
{
	git_vector *refs = &t->refs;
	git_pkt_ref *pkt;
	git_transport_caps *caps = &t->caps;
	const char *ptr;

	pkt = git_vector_get(refs, 0);
	/* No refs or capabilites, odd but not a problem */
	if (pkt == NULL || pkt->capabilities == NULL)
		return GIT_SUCCESS;

	ptr = pkt->capabilities;
	while (ptr != NULL && *ptr != '\0') {
		if (*ptr == ' ')
			ptr++;

		if(!git__prefixcmp(ptr, GIT_CAP_OFS_DELTA)) {
			caps->common = caps->ofs_delta = 1;
			ptr += strlen(GIT_CAP_OFS_DELTA);
			continue;
		}

		/* We don't know this capability, so skip it */
		ptr = strchr(ptr, ' ');
	}

	return GIT_SUCCESS;
}

/*
 * Since this is a network connection, we need to parse and store the
 * pkt-lines at this stage and keep them there.
 */
static int git_connect(git_transport *transport, int direction)
{
	transport_git *t = (transport_git *) transport;
	int error = GIT_SUCCESS;

	if (direction == GIT_DIR_PUSH)
		return git__throw(GIT_EINVALIDARGS, "Pushing is not supported with the git protocol");

	t->parent.direction = direction;
	error = git_vector_init(&t->refs, 16, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Connect and ask for the refs */
	error = do_connect(t, transport->url);
	if (error < GIT_SUCCESS)
		return error;

	gitno_buffer_setup(&t->buf, t->buff, sizeof(t->buff), t->socket);

	t->parent.connected = 1;
	error = store_refs(t);
	if (error < GIT_SUCCESS)
		return error;

	error = detect_caps(t);

cleanup:
	if (error < GIT_SUCCESS) {
		git_vector_free(&t->refs);
	}

	return error;
}

static int git_ls(git_transport *transport, git_headarray *array)
{
	transport_git *t = (transport_git *) transport;
	git_vector *refs = &t->refs;
	int len = 0;
	unsigned int i;

	array->heads = git__calloc(refs->length, sizeof(git_remote_head *));
	if (array->heads == NULL)
		return GIT_ENOMEM;

	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		if (p->type != GIT_PKT_REF)
			continue;

		++len;
		array->heads[i] = &(((git_pkt_ref *) p)->head);
	}
	array->len = len;
	t->heads = array->heads;

	return GIT_SUCCESS;
}

static int git_negotiate_fetch(git_transport *transport, git_repository *repo, git_headarray *wants)
{
	transport_git *t = (transport_git *) transport;
	git_revwalk *walk;
	git_reference *ref;
	git_strarray refs;
	git_oid oid;
	int error;
	unsigned int i;
	gitno_buffer *buf = &t->buf;

	error = git_pkt_send_wants(wants, &t->caps, t->socket);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to send wants list");

	error = git_reference_listall(&refs, repo, GIT_REF_LISTALL);
	if (error < GIT_ERROR)
		return git__rethrow(error, "Failed to list all references");

	error = git_revwalk_new(&walk, repo);
	if (error < GIT_ERROR) {
		error = git__rethrow(error, "Failed to list all references");
		goto cleanup;
	}
	git_revwalk_sorting(walk, GIT_SORT_TIME);

	for (i = 0; i < refs.count; ++i) {
		/* No tags */
		if (!git__prefixcmp(refs.strings[i], GIT_REFS_TAGS_DIR))
			continue;

		error = git_reference_lookup(&ref, repo, refs.strings[i]);
		if (error < GIT_ERROR) {
			error = git__rethrow(error, "Failed to lookup %s", refs.strings[i]);
			goto cleanup;
		}

		if (git_reference_type(ref) == GIT_REF_SYMBOLIC)
			continue;
		error = git_revwalk_push(walk, git_reference_oid(ref));
		if (error < GIT_ERROR) {
			error = git__rethrow(error, "Failed to push %s", refs.strings[i]);
			goto cleanup;
		}
	}
	git_strarray_free(&refs);

	/*
	 * We don't support any kind of ACK extensions, so the negotiation
	 * boils down to sending what we have and listening for an ACK
	 * every once in a while.
	 */
	i = 0;
	while ((error = git_revwalk_next(&oid, walk)) == GIT_SUCCESS) {
		error = git_pkt_send_have(&oid, t->socket);
		i++;
		if (i % 20 == 0) {
			const char *ptr = buf->data, *line_end;
			git_pkt *pkt;
			git_pkt_send_flush(t->socket);
			while (1) {
				/* Wait for max. 1 second */
				error = gitno_select_in(buf, 1, 0);
				if (error < GIT_SUCCESS) {
					error = git__throw(GIT_EOSERR, "Error in select");
				} else if (error == 0) {
				/*
				 * Some servers don't respond immediately, so if this
				 * happens, we keep sending information until it
				 * answers.
				 */
					break;
				}

				error = gitno_recv(buf);
				if (error < GIT_SUCCESS) {
				 error = git__rethrow(error, "Error receiving data");
				 goto cleanup;
				}
				error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
				if (error == GIT_ESHORTBUFFER)
					continue;
				if (error < GIT_SUCCESS) {
					error = git__rethrow(error, "Failed to get answer");
					goto cleanup;
				}

				gitno_consume(buf, line_end);

				if (pkt->type == GIT_PKT_ACK) {
					git__free(pkt);
					error = GIT_SUCCESS;
					goto done;
				} else if (pkt->type == GIT_PKT_NAK) {
					git__free(pkt);
					break;
				} else {
					error = git__throw(GIT_ERROR, "Got unexpected pkt type");
					goto cleanup;
				}
			}
		}
	}
	if (error == GIT_EREVWALKOVER)
		error = GIT_SUCCESS;

done:
	git_pkt_send_flush(t->socket);
	git_pkt_send_done(t->socket);

cleanup:
	git_revwalk_free(walk);

	return error;
}

static int git_send_flush(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_flush(t->socket);
}

static int git_send_done(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_done(t->socket);
}

static int git_download_pack(char **out, git_transport *transport, git_repository *repo)
{
	transport_git *t = (transport_git *) transport;
	int error = GIT_SUCCESS;
	gitno_buffer *buf = &t->buf;
	git_pkt *pkt;
	const char *line_end, *ptr;

	/*
	 * For now, we ignore everything and wait for the pack
	 */
	while (1) {
		ptr = buf->data;
		/* Whilst we're searching for the pack */
		while (1) {
			if (buf->offset == 0) {
				break;
			}

			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf->offset);
			if (error == GIT_ESHORTBUFFER)
				break;

			if (error < GIT_SUCCESS)
				return error;

			if (pkt->type == GIT_PKT_PACK) {
				git__free(pkt);
				return git_fetch__download_pack(out, buf->data, buf->offset, t->socket, repo);
			}

			/* For now we don't care about anything */
			git__free(pkt);
			gitno_consume(buf, line_end);
		}

		error = gitno_recv(buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(GIT_EOSERR, "Failed to receive data");
		if (error == 0) { /* Orderly shutdown */
			return GIT_SUCCESS;
		}

	}
}


static int git_close(git_transport *transport)
{
	transport_git *t = (transport_git*) transport;
	int error;

	/* Can't do anything if there's an error, so don't bother checking  */
	git_pkt_send_flush(t->socket);
	error = gitno_close(t->socket);

	if (error < 0)
		error = git__throw(GIT_EOSERR, "Failed to close socket");

#ifdef GIT_WIN32
	WSACleanup();
#endif

	return error;
}

static void git_free(git_transport *transport)
{
	transport_git *t = (transport_git *) transport;
	git_vector *refs = &t->refs;
	unsigned int i;

	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		git_pkt_free(p);
	}

	git_vector_free(refs);
	git__free(t->heads);
	git__free(t->parent.url);
	git__free(t);
}

int git_transport_git(git_transport **out)
{
	transport_git *t;
#ifdef GIT_WIN32
	int ret;
#endif

	t = git__malloc(sizeof(transport_git));
	if (t == NULL)
		return GIT_ENOMEM;

	memset(t, 0x0, sizeof(transport_git));

	t->parent.connect = git_connect;
	t->parent.ls = git_ls;
	t->parent.negotiate_fetch = git_negotiate_fetch;
	t->parent.send_flush = git_send_flush;
	t->parent.send_done = git_send_done;
	t->parent.download_pack = git_download_pack;
	t->parent.close = git_close;
	t->parent.free = git_free;

	*out = (git_transport *) t;

#ifdef GIT_WIN32
	ret = WSAStartup(MAKEWORD(2,2), &t->wsd);
	if (ret != 0) {
		git_free(*out);
		return git__throw(GIT_EOSERR, "Winsock init failed");
	}
#endif

	return GIT_SUCCESS;
}
