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

#ifndef _WIN32
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netdb.h>
#else
# define _WIN32_WINNT 0x0501
# include <winsock2.h>
# include <Ws2tcpip.h>
# pragma comment(lib, "Ws2_32.lib")
#endif

#include "git2/errors.h"

#include "common.h"
#include "netops.h"

void gitno_buffer_setup(gitno_buffer *buf, char *data, unsigned int len, int fd)
{
	memset(buf, 0x0, sizeof(gitno_buffer));
	memset(data, 0x0, len);
	buf->data = data;
	buf->len = len - 1;
	buf->offset = 0;
	buf->fd = fd;
}

int gitno_recv(gitno_buffer *buf)
{
	int ret;

	ret = recv(buf->fd, buf->data + buf->offset, buf->len - buf->offset, 0);
	if (ret < 0)
		return git__throw(GIT_EOSERR, "Failed to receive data: %s", strerror(errno));
	if (ret == 0) /* Orderly shutdown, so exit */
		return GIT_SUCCESS;

	buf->offset += ret;

	return ret;
}

/* Consume up to ptr and move the rest of the buffer to the beginning */
void gitno_consume(gitno_buffer *buf, const char *ptr)
{
	size_t consumed;

	assert(ptr - buf->data >= 0);
	assert(ptr - buf->data <= (int) buf->len);

	consumed = ptr - buf->data;

	memmove(buf->data, ptr, buf->offset - consumed);
	memset(buf->data + buf->offset, 0x0, buf->len - buf->offset);
	buf->offset -= consumed;
}

/* Consume const bytes and move the rest of the buffer to the beginning */
void gitno_consume_n(gitno_buffer *buf, size_t cons)
{
	memmove(buf->data, buf->data + cons, buf->len - buf->offset);
	memset(buf->data + cons, 0x0, buf->len - buf->offset);
	buf->offset -= cons;
}

int gitno_connect(const char *host, const char *port)
{
	struct addrinfo *info, *p;
	struct addrinfo hints;
	int ret, error = GIT_SUCCESS;
	int s;

	memset(&hints, 0x0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ret = getaddrinfo(host, port, &hints, &info);
	if (ret != 0) {
		error = GIT_EOSERR;
		goto cleanup;
	}

	for (p = info; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s < 0) {
			error = GIT_EOSERR;
			goto cleanup;
		}

		ret = connect(s, p->ai_addr, p->ai_addrlen);
		/* If we can't connect, try the next one */
		if (ret < 0) {
			continue;
		}

		/* Return the socket */
		error = s;
		goto cleanup;
	}

	/* Oops, we couldn't connect to any address */
	error = GIT_EOSERR;

cleanup:
	freeaddrinfo(info);
	return error;
}

int gitno_send(int s, const char *msg, size_t len, int flags)
{
	int ret;
	size_t off = 0;

	while (off < len) {
		ret = send(s, msg + off, len - off, flags);
		if (ret < 0)
			return GIT_EOSERR;

		off += ret;
	}

	return off;
}

int gitno_select_in(gitno_buffer *buf, long int sec, long int usec)
{
	fd_set fds;
	struct timeval tv;

	tv.tv_sec = sec;
	tv.tv_usec = usec;

	FD_ZERO(&fds);
	FD_SET(buf->fd, &fds);

	/* The select(2) interface is silly */
	return select(buf->fd + 1, &fds, NULL, NULL, &tv);
}
