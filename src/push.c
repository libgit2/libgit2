/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"

#include "common.h"
#include "pack.h"
#include "pack-objects.h"
#include "remote.h"
#include "vector.h"
#include "push.h"

int git_push_new(git_push **out, git_remote *remote)
{
	git_push *p;

	*out = NULL;

	p = git__calloc(1, sizeof(*p));
	GITERR_CHECK_ALLOC(p);

	p->repo = remote->repo;
	p->remote = remote;
	p->report_status = 1;

	if (git_vector_init(&p->specs, 0, NULL) < 0) {
		git__free(p);
		return -1;
	}

	if (git_vector_init(&p->status, 0, NULL) < 0) {
		git_vector_free(&p->specs);
		git__free(p);
		return -1;
	}

	*out = p;
	return 0;
}

static void free_refspec(push_spec *spec)
{
	if (spec == NULL)
		return;

	if (spec->lref)
		git__free(spec->lref);

	if (spec->rref)
		git__free(spec->rref);

	git__free(spec);
}

static void free_status(push_status *status)
{
	if (status == NULL)
		return;

	if (status->msg)
		git__free(status->msg);

	git__free(status->ref);
	git__free(status);
}

static int check_ref(char *ref)
{
	if (strcmp(ref, "HEAD") &&
	    git__prefixcmp(ref, "refs/heads/") &&
	    git__prefixcmp(ref, "refs/tags/")) {
		giterr_set(GITERR_INVALID, "No valid reference '%s'", ref);
		return -1;
	}
	return 0;
}

static int parse_refspec(push_spec **spec, const char *str)
{
	push_spec *s;
	char *delim;

	*spec = NULL;

	s = git__calloc(1, sizeof(*s));
	GITERR_CHECK_ALLOC(s);

	if (str[0] == '+') {
		s->force = true;
		str++;
	}

#define check(ref) \
	if (!ref || check_ref(ref) < 0) goto on_error

	delim = strchr(str, ':');
	if (delim == NULL) {
		s->lref = git__strdup(str);
		check(s->lref);
		s->rref = NULL;
	} else {
		if (delim - str) {
			s->lref = git__strndup(str, delim - str);
			check(s->lref);
		} else
			s->lref = NULL;

		if (strlen(delim + 1)) {
			s->rref = git__strdup(delim + 1);
			check(s->rref);
		} else
			s->rref = NULL;
	}

	if (!s->lref && !s->rref)
		goto on_error;

#undef check

	*spec = s;
	return 0;

on_error:
	free_refspec(s);
	return -1;
}

int git_push_add_refspec(git_push *push, const char *refspec)
{
	push_spec *spec;

	if (parse_refspec(&spec, refspec) < 0 ||
	    git_vector_insert(&push->specs, spec) < 0)
		return -1;

	return 0;
}

static int revwalk(git_vector *commits, git_push *push)
{
	git_remote_head *head;
	push_spec *spec;
	git_revwalk *rw;
	git_oid oid;
	unsigned int i;
	int error = -1;

	if (git_revwalk_new(&rw, push->repo) < 0)
		return -1;

	git_revwalk_sorting(rw, GIT_SORT_TIME);

	git_vector_foreach(&push->specs, i, spec) {
		if (git_oid_iszero(&spec->loid))
			/*
			 * Delete reference on remote side;
			 * nothing to do here.
			 */
			continue;

		if (git_oid_equal(&spec->loid, &spec->roid))
			continue; /* up-to-date */

		if (git_revwalk_push(rw, &spec->loid) < 0)
			goto on_error;

		if (!spec->force) {
			git_oid base;

			if (git_oid_iszero(&spec->roid))
				continue;

			if (!git_odb_exists(push->repo->_odb, &spec->roid)) {
				giterr_clear();
				error = GIT_ENONFASTFORWARD;
				goto on_error;
			}

			error = git_merge_base(&base, push->repo,
					       &spec->loid, &spec->roid);

			if (error == GIT_ENOTFOUND ||
				(!error && !git_oid_equal(&base, &spec->roid))) {
				giterr_clear();
				error = GIT_ENONFASTFORWARD;
				goto on_error;
			}

			if (error < 0)
				goto on_error;
		}
	}

	git_vector_foreach(&push->remote->refs, i, head) {
		if (git_oid_iszero(&head->oid))
			continue;

		/* TODO */
		git_revwalk_hide(rw, &head->oid);
	}

	while ((error = git_revwalk_next(&oid, rw)) == 0) {
		git_oid *o = git__malloc(GIT_OID_RAWSZ);
		GITERR_CHECK_ALLOC(o);
		git_oid_cpy(o, &oid);
		if (git_vector_insert(commits, o) < 0) {
			error = -1;
			goto on_error;
		}
	}

on_error:
	git_revwalk_free(rw);
	return error == GIT_ITEROVER ? 0 : error;
}

static int queue_objects(git_push *push)
{
	git_vector commits;
	git_oid *o;
	unsigned int i;
	int error;

	if (git_vector_init(&commits, 0, NULL) < 0)
		return -1;

	if ((error = revwalk(&commits, push)) < 0)
		goto on_error;

	if (!commits.length) {
		git_vector_free(&commits);
		return 0; /* nothing to do */
	}

	git_vector_foreach(&commits, i, o) {
		if ((error = git_packbuilder_insert(push->pb, o, NULL)) < 0)
			goto on_error;
	}

	git_vector_foreach(&commits, i, o) {
		git_object *obj;

		if ((error = git_object_lookup(&obj, push->repo, o, GIT_OBJ_ANY)) < 0)
			goto on_error;

		switch (git_object_type(obj)) {
		case GIT_OBJ_TAG: /* TODO: expect tags */
		case GIT_OBJ_COMMIT:
			if ((error = git_packbuilder_insert_tree(push->pb,
					git_commit_tree_id((git_commit *)obj))) < 0) {
				git_object_free(obj);
				goto on_error;
			}
			break;
		case GIT_OBJ_TREE:
		case GIT_OBJ_BLOB:
		default:
			git_object_free(obj);
			giterr_set(GITERR_INVALID, "Given object type invalid");
			error = -1;
			goto on_error;
		}
		git_object_free(obj);
	}
	error = 0;

on_error:
	git_vector_foreach(&commits, i, o) {
		git__free(o);
	}
	git_vector_free(&commits);
	return error;
}

static int calculate_work(git_push *push)
{
	git_remote_head *head;
	push_spec *spec;
	unsigned int i, j;

	git_vector_foreach(&push->specs, i, spec) {
		if (spec->lref) {
			if (git_reference_name_to_id(
					&spec->loid, push->repo, spec->lref) < 0) {
				giterr_set(GIT_ENOTFOUND, "No such reference '%s'", spec->lref);
				return -1;
			}

			if (!spec->rref) {
				/*
				 * No remote reference given; if we find a remote
				 * reference with the same name we will update it,
				 * otherwise a new reference will be created.
				 */
				git_vector_foreach(&push->remote->refs, j, head) {
					if (!strcmp(spec->lref, head->name)) {
						/*
						 * Update remote reference
						 */
						git_oid_cpy(&spec->roid, &head->oid);

						break;
					}
				}
			} else {
				/*
				 * Remote reference given; update the given
				 * reference or create it.
				 */
				git_vector_foreach(&push->remote->refs, j, head) {
					if (!strcmp(spec->rref, head->name)) {
						/*
						 * Update remote reference
						 */
						git_oid_cpy(&spec->roid, &head->oid);

						break;
					}
				}
			}
		}
	}

	return 0;
}

static int do_push(git_push *push)
{
	int error;
	git_transport *transport = push->remote->transport;

	/*
	 * A pack-file MUST be sent if either create or update command
	 * is used, even if the server already has all the necessary
	 * objects.  In this case the client MUST send an empty pack-file.
	 */

	if ((error = git_packbuilder_new(&push->pb, push->repo)) < 0 ||
		(error = calculate_work(push)) < 0 ||
		(error = queue_objects(push)) < 0 ||
		(error = transport->push(transport, push)) < 0)
		goto on_error;

	error = 0;

on_error:
	git_packbuilder_free(push->pb);
	return error;
}

static int cb_filter_refs(git_remote_head *ref, void *data)
{
	git_remote *remote = (git_remote *) data;
	return git_vector_insert(&remote->refs, ref);
}

static int filter_refs(git_remote *remote)
{
	git_vector_clear(&remote->refs);
	return git_remote_ls(remote, cb_filter_refs, remote);
}

int git_push_finish(git_push *push)
{
	int error;

	if (!git_remote_connected(push->remote) &&
		(error = git_remote_connect(push->remote, GIT_DIRECTION_PUSH)) < 0)
		return error;

	if ((error = filter_refs(push->remote)) < 0 ||
		(error = do_push(push)) < 0)
		return error;

	return 0;
}

int git_push_unpack_ok(git_push *push)
{
	return push->unpack_ok;
}

int git_push_status_foreach(git_push *push,
		int (*cb)(const char *ref, const char *msg, void *data),
		void *data)
{
	push_status *status;
	unsigned int i;

	git_vector_foreach(&push->status, i, status) {
		if (cb(status->ref, status->msg, data) < 0)
			return GIT_EUSER;
	}

	return 0;
}

void git_push_free(git_push *push)
{
	push_spec *spec;
	push_status *status;
	unsigned int i;

	if (push == NULL)
		return;

	git_vector_foreach(&push->specs, i, spec) {
		free_refspec(spec);
	}
	git_vector_free(&push->specs);

	git_vector_foreach(&push->status, i, status) {
		free_status(status);
	}
	git_vector_free(&push->status);

	git__free(push);
}
