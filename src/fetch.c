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

/*
 * Don't forget that this depends on the enum being correctly set
 */
static int whn_cmp(const void *a, const void *b)
{
	git_remote_head *heada = (git_remote_head *) a;
	git_remote_head *headb = (git_remote_head *) b;

	return headb->type - heada->type;
}

/*
 * FIXME: we assume that the transport has been connected, enforce
 * that somehow, we also want to be called from _negotiate
 */
int git_fetch_list_want(git_headarray *whn_list, git_repository *repo, git_remote *remote)
{
	git_vector list;
	git_headarray refs, lrefs;
	git_transport *t = remote->transport;
	const git_refspec *spec;
	int error;
	unsigned int i;

	error = git_vector_init(&list, 16, whn_cmp);
	if (error < GIT_SUCCESS)
		return error;

	error = git_transport_ls(t, &refs);
	if (error < GIT_SUCCESS) {
		error = git__rethrow(error, "Failed to list local refs");
		goto cleanup;
	}

	spec = git_remote_fetchspec(remote);
	if (spec == NULL) {
		error = git__throw(GIT_ERROR, "The remote has to fetchspec");
		goto cleanup;
	}

	for (i = 0; i < refs.len; ++i) {
		char local[1024];
		git_reference *ref;
		git_remote_head *head = refs.heads[i];

		/* If it doesn't match the refpec, we don't want it */
		error = git_refspec_src_match(spec, head->name);
		if (error == GIT_ENOMATCH)
			continue;
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Error matching remote ref name");
			goto cleanup;
		}

		/* If the local ref is the same, we don't want it either */
		error = git_refspec_transform(local, sizeof(local), spec, head->name);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Error transforming ref name");
			goto cleanup;
		}

		error = git_reference_lookup(&ref, repo, local);
		/* If we don't have it locally, it's new, so we want it */
		if (error < GIT_SUCCESS && error != GIT_ENOTFOUND) {
			error = git__rethrow(error, "Error looking up local ref");
			goto cleanup;
		}

		if (ref != NULL) {
			if (!git_oid_cmp(&head->oid, git_reference_oid(ref)))
				continue;

			head->local = 1;
			git_oid_cpy(&head->loid, git_reference_oid(ref));
		}

		/*
		 * Now we know we want to have that ref, so add it as a "want"
		 * to the list, storing the local oid for that branch so we
		 * don't have to look for it again.
		 */
		head->type = GIT_WHN_WANT;
		error = git_vector_insert(&list, head);
		if (error < GIT_SUCCESS)
			goto cleanup;
	}

	git_vector_sort(&list);
	whn_list->len = list.length;
	whn_list->heads = (git_remote_head **) list.contents;

	return GIT_SUCCESS;

cleanup:
	git_vector_free(&list);
	return error;
}

/* Push any (OID) ref it gets into the walker */
static int push_stuff(const char *name, void *data)
{
	git_revwalk *walk = (git_revwalk *) data;
	git_reference *ref;
	git_repository *repo;
	int error;

	repo = git_revwalk_repository(walk);
	error = git_reference_lookup(&ref, repo, name);
	if (error < GIT_SUCCESS)
		return error;

	return git_revwalk_push(walk, git_reference_oid(ref));
}

/*
 * In this first version, we push all our refs in and start sending
 * them out. When we get an ACK we hide that commit and continue
 * traversing until we're done
 */
int git_fetch_negotiate(git_headarray *list, git_repository *repo, git_remote *remote)
{
	git_revwalk *walk;
	int error;
	unsigned int i;
	char local[1024];
	git_refspec *spec;

	error = git_revwalk_new(&walk, repo);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create walker");

	for (i = 0; i < list->len; ++i) {
		git_reference *ref;
		git_remote_head *head = list->heads[i];

		if (!head->local)
			continue;

		error = git_revwalk_push(walk, &head->loid);
		if (error < GIT_SUCCESS) {
			error = git__rethrow(error, "Failed to push a local OID");
			goto cleanup;
		}
	}

	/*
	 * Now we have everything set up so we can start tell the server
	 * what we want and what we have.
	 */
	git_transport_send_wants(remote->transport, list);
	git_transport_send_haves(remote->transport, repo);

cleanup:
	git_revwalk_free(walk);
	return error;
}

int git_fetch_download_pack(git_remote *remote)
{
	return GIT_ENOTIMPLEMENTED;
}
