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

#ifndef _MSC_VER
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
#else
# include <winsock2.h>
# include <Ws2tcpip.h>
# pragma comment(lib, "Ws2_32.lib")
#endif

#include "git2/net.h"
#include "git2/pkt.h"
#include "git2/common.h"
#include "git2/types.h"
#include "git2/errors.h"

#include "vector.h"
#include "transport.h"
#include "common.h"

typedef struct {
	int socket;
	git_vector refs;
	git_remote_head **heads;
} git_priv;

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
static int do_connect(git_priv *priv, const char *url)
{
	int s = -1;
	char *host, *port, *msg;
	const char prefix[] = "git://";
	int error, ret, msg_len, connected = 0;
	struct addrinfo *info, *p;
	struct addrinfo hints;

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP */

	if (!git__prefixcmp(url, prefix))
		url += STRLEN(prefix);

	error = extract_host_and_port(&host, &port, url);

	ret = getaddrinfo(host, port, &hints, &info);
	if (ret != 0) {
		info = NULL;
		error = git__throw(GIT_EOSERR, "Failed to get address info: %s", gai_strerror(ret));
		goto cleanup;
	}

	for (p = info; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s < 0) {
			error = git__throw(GIT_EOSERR, "Failed to create socket");
			goto cleanup;
		}

		ret = connect(s, p->ai_addr, p->ai_addrlen);
		if (ret < 0) { /* Try the next one */
			continue;
		}
		connected = 1;

		error = git_pkt_gen_proto(&msg, &msg_len, url);
		if (error < GIT_SUCCESS)
			break;

		/* FIXME: Do this in a loop */
		ret = send(s, msg, msg_len, 0);
		free(msg);
		if (ret < 0)
			error = git__throw(GIT_EOSERR, "Failed to send request");
	}

	priv->socket = s;

cleanup:
	freeaddrinfo(info);
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
static int store_refs(git_priv *priv)
{
	int s = priv->socket;
	git_vector *refs = &priv->refs;
	int error = GIT_SUCCESS;
	char buffer[1024] = {0};
	const char *line_end, *ptr;
	int off = 0, ret;
	git_pkt *pkt;

	while (1) {
		ret = recv(s, buffer, sizeof(buffer) - off, 0);
		if (ret < 0)
			return git__throw(GIT_EOSERR, "Failed to receive data");
		if (ret == 0)
			puts("got zero!");

		ptr = buffer;
		while (1) {
			error = git_pkt_parse_line(&pkt, ptr, &line_end);
			/*
			 * An error here can mean that the input in corrupt or
			 * (more likely) that the input hasn't arrived yet.
			 *
			 * FIXME: Add actual error checking. Right now we just ask
			 * for more input.
			 */
			if (error < GIT_SUCCESS)
				break;

			error = git_vector_insert(refs, pkt);
			if (error < GIT_SUCCESS)
				return error;

			if (pkt->type != GIT_PKT_REF)
				return GIT_SUCCESS;

			ptr = line_end;
		}

		/*
		 * Move the rest to the start of the buffer
		 */
		memmove(buffer, line_end, sizeof(buffer) - (line_end - buffer));
		off = ret - (line_end - buffer);
	}

	return error;
}

/*
 * Since this is a network connection, we need to parse and store the
 * pkt-lines at this stage and keep them there.
 */
static int git_connect(git_transport *transport, git_net_direction direction)
{
	git_priv *priv;
	int error = GIT_SUCCESS;

	if (direction == INTENT_PUSH)
		return git__throw(GIT_EINVALIDARGS, "Pushing is not supported with the git protocol");

	priv = git__malloc(sizeof(git_priv));
	if (priv == NULL)
		return GIT_ENOMEM;

	memset(priv, 0x0, sizeof(git_priv));
	transport->private = priv;
	error = git_vector_init(&priv->refs, 16, NULL);
	if (error < GIT_SUCCESS)
		goto cleanup;

	/* Connect and ask for the refs */
	error = do_connect(priv, transport->url);
	if (error < GIT_SUCCESS)
		return error;

	error = store_refs(priv);

cleanup:
	if (error < GIT_SUCCESS) {
		git_vector_free(&priv->refs);
		free(priv);
	}

	return error;
}

static int git_ls(git_transport *transport, git_headarray *array)
{
	git_priv *priv = transport->private;
	git_vector *refs = &priv->refs;
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
	priv->heads = array->heads;

	return GIT_SUCCESS;
}

static int git_close(git_transport *transport)
{
	git_priv *priv = transport->private;
	int s = priv->socket;
	int error;

	error = close(s);
	if (error < 0)
		error = git__throw(GIT_EOSERR, "Failed to close socket");

	return error;
}

static void git_free(git_transport *transport)
{
	git_priv *priv = transport->private;
	git_vector *refs = &priv->refs;
	unsigned int i;

	for (i = 0; i < refs->length; ++i) {
		git_pkt *p = git_vector_get(refs, i);
		if (p->type == GIT_PKT_REF) {
			free(((git_pkt_ref *)p)->head.name);
			free(((git_pkt_ref *)p)->capabilities);
		}

		free(p);
	}

	git_vector_free(refs);
	free(priv->heads);
	free(priv);
	free(transport->url);
	free(transport);
}

int git_transport_git(git_transport *transport)
{
	transport->connect = git_connect;
	transport->ls = git_ls;
	transport->close = git_close;
	transport->free = git_free;

	return GIT_SUCCESS;
}
