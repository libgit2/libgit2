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
#include "git2/pkt.h"
#include "git2/common.h"
#include "git2/types.h"
#include "git2/errors.h"

#include "vector.h"
#include "transport.h"
#include "common.h"
#include "netops.h"

typedef struct {
	git_transport parent;
	int socket;
	git_vector refs;
	git_remote_head **heads;
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

	len = 4 + strlen(cmd) + 1 + strlen(repo) + 1 + STRLEN(host) + (delim - url) + 2;

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
		url += STRLEN(prefix);

	error = extract_host_and_port(&host, &port, url);
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
	t->parent.close = git_close;
	t->parent.free = git_free;

	*out = (git_transport *) t;

	return GIT_SUCCESS;
}
