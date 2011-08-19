/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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

typedef struct {
	git_transport parent;
	int socket;
	git_vector refs;
	git_remote_head **heads;
	git_transport_caps caps;
} transport_git;

/*
 * Create a git procol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 */
static int gen_proto(char **out, int *outlen, const char *cmd, const char *url)
{
	char *delim, *repo, *ptr;
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

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + strlen(host) + (delim - url) + 2;

	*out = git__malloc(len);
	if (*out == NULL)
		return GIT_ENOMEM;

	*outlen = len - 1;
	ptr = *out;
	memset(ptr, 0x0, len);
	/* We expect the return value to be > len - 1 so don't bother checking it */
	snprintf(ptr, len -1, "%04x%s %s%c%s%s", len - 1, cmd, repo, 0, host, url);

	return GIT_SUCCESS;
}

static int send_request(int s, const char *cmd, const char *url)
{
	int error, len;
	char *msg = NULL;

	error = gen_proto(&msg, &len, cmd, url);
	if (error < GIT_SUCCESS)
		goto cleanup;

	error = gitno_send(s, msg, len, 0);

cleanup:
	free(msg);
	return error;
}

/* The URL should already have been stripped of the protocol */
static int extract_host_and_port(char **host, char **port, const char *url)
{
	char *colon, *slash, *delim;
	int error = GIT_SUCCESS;

	colon = strchr(url, ':');
	slash = strchr(url, '/');

	if (slash == NULL)
			return git__throw(GIT_EOBJCORRUPTED, "Malformed URL: missing /");

	if (colon == NULL) {
		*port = git__strdup(GIT_DEFAULT_PORT);
	} else {
		*port = git__strndup(colon + 1, slash - colon - 1);
	}
	if (*port == NULL)
		return GIT_ENOMEM;;


	delim = colon == NULL ? slash : colon;
	*host = git__strndup(url, delim - url);
	if (*host == NULL) {
		free(*port);
		error = GIT_ENOMEM;
	}

	return error;
}

/*
 * Parse the URL and connect to a server, storing the socket in
 * out. For convenience this also takes care of asking for the remote
 * refs
 */
static int do_connect(transport_git *t, const char *url)
{
	int s = -1;
	char *host, *port;
	const char prefix[] = "git://";
	int error, connected = 0;

	if (!git__prefixcmp(url, prefix))
		url += strlen(prefix);

	error = extract_host_and_port(&host, &port, url);
	if (error < GIT_SUCCESS)
		return error;
	s = gitno_connect(host, port);
	connected = 1;
	error = send_request(s, NULL, url);
	t->socket = s;

	free(host);
	free(port);

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
	gitno_buffer buf;
	int s = t->socket;
	git_vector *refs = &t->refs;
	int error = GIT_SUCCESS;
	char buffer[1024];
	const char *line_end, *ptr;
	git_pkt *pkt;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), s);

	while (1) {
		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(GIT_EOSERR, "Failed to receive data");
		if (error == GIT_SUCCESS) /* Orderly shutdown, so exit */
			return GIT_SUCCESS;

		ptr = buf.data;
		while (1) {
			if (buf.offset == 0)
				break;
			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf.offset);
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
			gitno_consume(&buf, line_end);

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

static int git_send_wants(git_transport *transport, git_headarray *array)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_wants(array, &t->caps, t->socket);
}

static int git_send_have(git_transport *transport, git_oid *oid)
{
	transport_git *t = (transport_git *) transport;

	return git_pkt_send_have(oid, t->socket);
}

static int git_negotiate_fetch(git_transport *transport, git_repository *repo, git_headarray *GIT_UNUSED(list))
{
	transport_git *t = (transport_git *) transport;
	git_revwalk *walk;
	git_reference *ref;
	git_strarray refs;
	git_oid oid;
	int error;
	unsigned int i;
	char buff[128];
	gitno_buffer buf;
	GIT_UNUSED_ARG(list);

	gitno_buffer_setup(&buf, buff, sizeof(buff), t->socket);

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
			const char *ptr = buf.data, *line_end;
			git_pkt *pkt;
			git_pkt_send_flush(t->socket);
			while (1) {
				/* Wait for max. 1 second */
				error = gitno_select_in(&buf, 1, 0);
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

				error = gitno_recv(&buf);
				if (error < GIT_SUCCESS) {
				  error = git__rethrow(error, "Error receiving data");
				  goto cleanup;
				}
				error = git_pkt_parse_line(&pkt, ptr, &line_end, buf.offset);
				if (error == GIT_ESHORTBUFFER)
					continue;
				if (error < GIT_SUCCESS) {
					error = git__rethrow(error, "Failed to get answer");
					goto cleanup;
				}

				gitno_consume(&buf, line_end);

				if (pkt->type == GIT_PKT_ACK) {
					error = GIT_SUCCESS;
					goto done;
				} else if (pkt->type == GIT_PKT_NAK) {
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

static int store_pack(char **out, gitno_buffer *buf, git_repository *repo)
{
	git_filebuf file;
	int error;
	char path[GIT_PATH_MAX], suff[] = "/objects/pack/pack-received\0";
	off_t off = 0;

	strcpy(path, repo->path_repository);
	off += strlen(repo->path_repository);
	strcat(path, suff);
	//memcpy(path + off, suff, GIT_PATH_MAX - off - strlen(suff) - 1);

	if (memcmp(buf->data, "PACK", strlen("PACK"))) {
		return git__throw(GIT_ERROR, "The pack doesn't start with the signature");
	}

	error = git_filebuf_open(&file, path, GIT_FILEBUF_TEMPORARY);
	if (error < GIT_SUCCESS)
		goto cleanup;

	while (1) {
		/* Part of the packfile has been received, don't loose it */
		error = git_filebuf_write(&file, buf->data, buf->offset);
		if (error < GIT_SUCCESS)
			goto cleanup;

		gitno_consume_n(buf, buf->offset);
		error = gitno_recv(buf);
		if (error < GIT_SUCCESS)
			goto cleanup;
		if (error == 0) /* Orderly shutdown */
			break;
	}

	*out = git__strdup(file.path_lock);
	if (*out == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	/* A bit dodgy, but we need to keep the pack at the temporary path */
	error = git_filebuf_commit_at(&file, file.path_lock);
cleanup:
	if (error < GIT_SUCCESS)
		git_filebuf_cleanup(&file);

	return error;
}

static int git_download_pack(char **out, git_transport *transport, git_repository *repo)
{
	transport_git *t = (transport_git *) transport;
	int s = t->socket, error = GIT_SUCCESS;
	gitno_buffer buf;
	char buffer[1024];
	git_pkt *pkt;
	const char *line_end, *ptr;

	gitno_buffer_setup(&buf, buffer, sizeof(buffer), s);
	/*
	 * For now, we ignore everything and wait for the pack
	 */
	while (1) {
		error = gitno_recv(&buf);
		if (error < GIT_SUCCESS)
			return git__rethrow(GIT_EOSERR, "Failed to receive data");
		if (error == 0) /* Orderly shutdown */
			return GIT_SUCCESS;

		ptr = buf.data;
		/* Whilst we're searching for the pack */
		while (1) {
			if (buf.offset == 0)
				break;
			error = git_pkt_parse_line(&pkt, ptr, &line_end, buf.offset);
			if (error == GIT_ESHORTBUFFER)
				break;
			if (error < GIT_SUCCESS)
				return error;

			if (pkt->type == GIT_PKT_PACK)
				return store_pack(out, &buf, repo);

			/* For now we don't care about anything */
			free(pkt);
			gitno_consume(&buf, line_end);
		}
	}
}


static int git_close(git_transport *transport)
{
	transport_git *t = (transport_git*) transport;
	int s = t->socket;
	int error;

	/* Can't do anything if there's an error, so don't bother checking  */
	git_pkt_send_flush(s);
	error = close(s);
	if (error < 0)
		error = git__throw(GIT_EOSERR, "Failed to close socket");

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
	free(t->heads);
	free(t->parent.url);
	free(t);
}

int git_transport_git(git_transport **out)
{
	transport_git *t;

	t = git__malloc(sizeof(transport_git));
	if (t == NULL)
		return GIT_ENOMEM;

	memset(t, 0x0, sizeof(transport_git));

	t->parent.connect = git_connect;
	t->parent.ls = git_ls;
	t->parent.send_wants = git_send_wants;
	t->parent.send_have = git_send_have;
	t->parent.negotiate_fetch = git_negotiate_fetch;
	t->parent.send_flush = git_send_flush;
	t->parent.send_done = git_send_done;
	t->parent.download_pack = git_download_pack;
	t->parent.close = git_close;
	t->parent.free = git_free;

	*out = (git_transport *) t;

	return GIT_SUCCESS;
}
