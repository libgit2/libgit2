/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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

static int push_spec_rref_cmp(const void *a, const void *b)
{
	const push_spec *push_spec_a = a, *push_spec_b = b;

	return strcmp(push_spec_a->rref, push_spec_b->rref);
}

static int push_status_ref_cmp(const void *a, const void *b)
{
	const push_status *push_status_a = a, *push_status_b = b;

	return strcmp(push_status_a->ref, push_status_b->ref);
}

int git_push_new(git_push **out, git_remote *remote)
{
	git_push *p;

	*out = NULL;

	p = git__calloc(1, sizeof(*p));
	GITERR_CHECK_ALLOC(p);

	p->repo = remote->repo;
	p->remote = remote;
	p->report_status = 1;
	p->pb_parallelism = 1;

	if (git_vector_init(&p->specs, 0, push_spec_rref_cmp) < 0) {
		git__free(p);
		return -1;
	}

	if (git_vector_init(&p->status, 0, push_status_ref_cmp) < 0) {
		git_vector_free(&p->specs);
		git__free(p);
		return -1;
	}

	*out = p;
	return 0;
}

int git_push_set_options(git_push *push, const git_push_options *opts)
{
	if (!push || !opts)
		return -1;

	GITERR_CHECK_VERSION(opts, GIT_PUSH_OPTIONS_VERSION, "git_push_options");

	push->pb_parallelism = opts->pb_parallelism;

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

static int check_rref(char *ref)
{
	if (git__prefixcmp(ref, "refs/")) {
		giterr_set(GITERR_INVALID, "Not a valid reference '%s'", ref);
		return -1;
	}

	return 0;
}

static int check_lref(git_push *push, char *ref)
{
	/* lref must be resolvable to an existing object */
	git_object *obj;
	int error = git_revparse_single(&obj, push->repo, ref);

	if (error) {
		if (error == GIT_ENOTFOUND)
			giterr_set(GITERR_REFERENCE,
				"src refspec '%s' does not match any existing object", ref);
		else
			giterr_set(GITERR_INVALID, "Not a valid reference '%s'", ref);

		return -1;
	} else
		git_object_free(obj);

	return 0;
}

static int parse_refspec(git_push *push, push_spec **spec, const char *str)
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

	delim = strchr(str, ':');
	if (delim == NULL) {
		s->lref = git__strdup(str);
		if (!s->lref || check_lref(push, s->lref) < 0)
			goto on_error;
	} else {
		if (delim - str) {
			s->lref = git__strndup(str, delim - str);
			if (!s->lref || check_lref(push, s->lref) < 0)
				goto on_error;
		}

		if (strlen(delim + 1)) {
			s->rref = git__strdup(delim + 1);
			if (!s->rref || check_rref(s->rref) < 0)
				goto on_error;
		}
	}

	if (!s->lref && !s->rref)
		goto on_error;

	/* If rref is ommitted, use the same ref name as lref */
	if (!s->rref) {
		s->rref = git__strdup(s->lref);
		if (!s->rref || check_rref(s->rref) < 0)
			goto on_error;
	}

	*spec = s;
	return 0;

on_error:
	free_refspec(s);
	return -1;
}

int git_push_add_refspec(git_push *push, const char *refspec)
{
	push_spec *spec;

	if (parse_refspec(push, &spec, refspec) < 0 ||
	    git_vector_insert(&push->specs, spec) < 0)
		return -1;

	return 0;
}

int git_push_update_tips(git_push *push)
{
	git_refspec *fetch_spec = &push->remote->fetch;
	git_buf remote_ref_name = GIT_BUF_INIT;
	size_t i, j;
	push_spec *push_spec;
	git_reference *remote_ref;
	push_status *status;
	int error = 0;

	git_vector_foreach(&push->status, i, status) {
		/* If this ref update was successful (ok, not ng), it will have an empty message */
		if (status->msg)
			continue;

		/* Find the corresponding remote ref */
		if (!git_refspec_src_matches(fetch_spec, status->ref))
			continue;

		if ((error = git_refspec_transform_r(&remote_ref_name, fetch_spec, status->ref)) < 0)
			goto on_error;

		/* Find matching  push ref spec */
		git_vector_foreach(&push->specs, j, push_spec) {
			if (!strcmp(push_spec->rref, status->ref))
				break;
		}

		/* Could not find the corresponding push ref spec for this push update */
		if (j == push->specs.length)
			continue;

		/* Update the remote ref */
		if (git_oid_iszero(&push_spec->loid)) {
			error = git_reference_lookup(&remote_ref, push->remote->repo, git_buf_cstr(&remote_ref_name));

			if (!error) {
				if ((error = git_reference_delete(remote_ref)) < 0)
					goto on_error;
			} else if (error == GIT_ENOTFOUND)
				giterr_clear();
			else
				goto on_error;
		} else if ((error = git_reference_create(NULL, push->remote->repo, git_buf_cstr(&remote_ref_name), &push_spec->loid, 1)) < 0)
			goto on_error;
	}

	error = 0;

on_error:
	git_buf_free(&remote_ref_name);
	return error;
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
		git_otype type;
		size_t size;

		if (git_oid_iszero(&spec->loid))
			/*
			 * Delete reference on remote side;
			 * nothing to do here.
			 */
			continue;

		if (git_oid_equal(&spec->loid, &spec->roid))
			continue; /* up-to-date */

		if (git_odb_read_header(&size, &type, push->repo->_odb, &spec->loid) < 0)
			goto on_error;

		if (type == GIT_OBJ_TAG) {
			git_tag *tag;
			git_object *target;

			if (git_packbuilder_insert(push->pb, &spec->loid, NULL) < 0)
				goto on_error;

			if (git_tag_lookup(&tag, push->repo, &spec->loid) < 0)
				goto on_error;

			if (git_tag_peel(&target, tag) < 0) {
				git_tag_free(tag);
				goto on_error;
			}
			git_tag_free(tag);

			if (git_object_type(target) == GIT_OBJ_COMMIT) {
				if (git_revwalk_push(rw, git_object_id(target)) < 0) {
					git_object_free(target);
					goto on_error;
				}
			} else {
				if (git_packbuilder_insert(
					push->pb, git_object_id(target), NULL) < 0) {
					git_object_free(target);
					goto on_error;
				}
			}
			git_object_free(target);
		} else if (git_revwalk_push(rw, &spec->loid) < 0)
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

	/* Update local and remote oids*/

	git_vector_foreach(&push->specs, i, spec) {
		if (spec->lref) {
			/* This is a create or update.  Local ref must exist. */
			if (git_reference_name_to_id(
					&spec->loid, push->repo, spec->lref) < 0) {
				giterr_set(GIT_ENOTFOUND, "No such reference '%s'", spec->lref);
				return -1;
			}
		}

		if (spec->rref) {
			/* Remote ref may or may not (e.g. during create) already exist. */
			git_vector_foreach(&push->remote->refs, j, head) {
				if (!strcmp(spec->rref, head->name)) {
					git_oid_cpy(&spec->roid, &head->oid);
					break;
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

	if (!transport->push) {
		giterr_set(GITERR_NET, "Remote transport doesn't support push");
		error = -1;
		goto on_error;
	}

	/*
	 * A pack-file MUST be sent if either create or update command
	 * is used, even if the server already has all the necessary
	 * objects.  In this case the client MUST send an empty pack-file.
	 */

	if ((error = git_packbuilder_new(&push->pb, push->repo)) < 0)
		goto on_error;

	git_packbuilder_set_threads(push->pb, push->pb_parallelism);

	if ((error = calculate_work(push)) < 0 ||
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
