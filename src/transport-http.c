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

#include <stdlib.h>

#include "transport.h"
#include "common.h"

typedef struct {
	git_transport parent;
} transport_http;

static void http_free(git_transport *transport)
{
	transport_http *t = (transport_http *) transport;

	free(t);
}


int git_transport_http(git_transport **out)
{
	transport_http *t;

	t = git__malloc(sizeof(transport_http));
	if (t == NULL)
		return GIT_ENOMEM;

	memset(t, 0x0, sizeof(transport_http));

	t->parent.free = http_free;

	*out = (git_transport *) t;

	return GIT_SUCCESS;
}
