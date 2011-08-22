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

#include "git2/remote.h"
#include "git2/oid.h"
#include "git2/refs.h"
#include "git2/revwalk.h"

#include "common.h"
#include "transport.h"
#include "remote.h"
#include "refspec.h"
#include "fetch.h"

static int filter_wants(git_remote *remote)
{
	git_vector list;
	git_headarray refs;
	git_transport *t = remote->transport;
	git_repository *repo = remote->repo;
	const git_refspec *spec;
	int error;
	unsigned int i;

	error = git_vector_init(&list, 16, NULL);
	if (error < GIT_SUCCESS)
		return error;

	error = t->ls(t, &refs);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to get remote ref list");
		goto cleanup;
	}

	spec = git_remote_fetchspec(remote);
	if (spec == NULL) {
		error = git__throw(GIT_ERROR, "The remote has no fetchspec");
		goto cleanup;
	}

	for (i = 0; i < refs.len; ++i) {
		git_remote_head *head = refs.heads[i];

		/* If it doesn't match the refpec, we don't want it */
		error = git_refspec_src_match(spec, head->name);
		if (error == GIT_ENOMATCH)
			continue;
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Error matching remote ref name");
			goto cleanup;
		}

		/* If we have the object, mark it so we don't ask for it */
		if (git_odb_exists(repo->db, &head->oid))
			head->local = 1;
		else
			remote->need_pack = 1;

		error = git_vector_insert(&list, head);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	remote->refs.len = list.length;
	remote->refs.heads = (git_remote_head **) list.contents;

	return GIT_SUCCESS;

cleanup:
	git_vector_free(&list);
	return error;
}

/*
 * In this first version, we push all our refs in and start sending
 * them out. When we get an ACK we hide that commit and continue
 * traversing until we're done
 */
int git_fetch_negotiate(git_remote *remote)
{
	int error;
	git_headarray *list = &remote->refs;
	git_transport *t = remote->transport;

	error = filter_wants(remote);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to filter the reference list for wants");

	/* Don't try to negotiate when we don't want anything */
	if (list->len == 0)
		return GIT_SUCCESS;
	if (!remote->need_pack)
		return GIT_SUCCESS;

	/*
	 * Now we have everything set up so we can start tell the server
	 * what we want and what we have.
	 */
	error = t->send_wants(t, list);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to send want list");

	return t->negotiate_fetch(t, remote->repo, &remote->refs);
}

int git_fetch_download_pack(char **out, git_remote *remote)
{
	if(!remote->need_pack) {
		*out = NULL;
		return GIT_SUCCESS;
	}

	return remote->transport->download_pack(out, remote->transport, remote->repo);
}
