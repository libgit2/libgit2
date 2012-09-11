/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "pack.h"
#include "pack-objects.h"
#include "remote.h"
#include "transport.h"
#include "vector.h"

#include "git2/commit.h"
#include "git2/index.h"
#include "git2/pack.h"
#include "git2/push.h"
#include "git2/remote.h"
#include "git2/revwalk.h"
#include "git2/tree.h"
#include "git2/version.h"

typedef struct push_spec {
	char *lref;
	char *rref;

	git_oid loid;
	git_oid roid;

	bool force;
} push_spec;

struct git_push {
	git_repository *repo;
	git_packbuilder *pb;
	git_remote *remote;
	git_vector specs;
};

int git_push_new(git_push **out, git_remote *remote)
{
	git_push *p;

	*out = NULL;

	p = git__malloc(sizeof(*p));
	GITERR_CHECK_ALLOC(p);
	memset(p, 0x0, sizeof(*p));

	p->repo = remote->repo;
	p->remote = remote;

	if (git_vector_init(&p->specs, 0, NULL) < 0) {
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

static int parse_refspec(push_spec **spec, const char *str)
{
	push_spec *s;
	char *delim;

	*spec = NULL;

	s = git__malloc(sizeof(*s));
	GITERR_CHECK_ALLOC(s);
	memset(s, 0x0, sizeof(*s));

	if (str[0] == '+') {
		s->force = true;
		str++;
	}

	delim = strchr(str, ':');
	if (delim == NULL) {
		if (strlen(str)) {
			s->lref = git__strdup(str);
			GITERR_CHECK_ALLOC(s->lref);
		} else
			s->lref = NULL;
		s->rref = NULL;
	} else {
		if (delim - str) {
			s->lref = git__strndup(str, delim - str);
			GITERR_CHECK_ALLOC(s->lref);
		} else
			s->lref = NULL;
		if (strlen(delim + 1)) {
			s->rref = git__strdup(delim + 1);
			GITERR_CHECK_ALLOC(s->rref);
		} else
			s->rref = NULL;
	}
	*spec = s;
	return 0;
}

int git_push_add_refspec(git_push *push, const char *refspec)
{
	push_spec *spec;

	if (strchr(refspec, '*')) {
		giterr_set(GITERR_INVALID, "No wildcard refspec supported");
		return -1;
	}

	if (parse_refspec(&spec, refspec) ||
	    git_vector_insert(&push->specs, spec) < 0) {
		free_refspec(spec);
		return -1;
	}

	return 0;
}

static int gen_pktline(git_buf *buf, git_push *push)
{
	git_remote_head *head;
	push_spec *spec;
	unsigned int i, j, len;
	char hex[41]; hex[40] = '\0';

	git_vector_foreach(&push->specs, i, spec) {
		len = 2*GIT_OID_HEXSZ + 7;

		if (spec->lref) {
			if (git_reference_name_to_oid(&spec->loid, push->repo,
						      spec->lref) < 0)
				return -1;

			if (!spec->rref) {
				/*
				 * No remote reference given; if we find a remote
				 * reference with the same name we will update it,
				 * otherwise a new reference will be created.
				 */
				len += strlen(spec->lref);
				git_vector_foreach(&push->remote->refs, j, head) {
					if (!strcmp(spec->lref, head->name)) {
						/* update remote reference */
						git_oid_cpy(&spec->roid, &head->oid);
						git_oid_fmt(hex, &spec->roid);
						git_buf_printf(buf, "%04x%s ", len, hex);

						git_oid_fmt(hex, &spec->loid);
						git_buf_printf(buf, "%s %s\n", hex,
							       spec->lref);

						break;
					}
				}
				if (git_oid_iszero(&spec->roid)) {
					/* create remote reference */
					git_oid_fmt(hex, &spec->loid);
					git_buf_printf(buf, "%04x%s %s %s\n", len,
						       GIT_OID_HEX_ZERO, hex, spec->lref);
				}
			} else {
				/*
				 * Remote reference given; update the given
				 * reference or create it.
				 */
				len += strlen(spec->rref);
				git_vector_foreach(&push->remote->refs, j, head) {
					if (!strcmp(spec->rref, head->name)) {
						/* update remote reference */
						git_oid_cpy(&spec->roid, &head->oid);
						git_oid_fmt(hex, &spec->roid);
						git_buf_printf(buf, "%04x%s ", len, hex);

						git_oid_fmt(hex, &spec->loid);
						git_buf_printf(buf, "%s %s\n", hex,
							       spec->rref);

						break;
					}
				}
				if (git_oid_iszero(&spec->roid)) {
					/* create remote reference */
					git_oid_fmt(hex, &spec->loid);
					git_buf_printf(buf, "%04x%s %s %s\n", len,
						       GIT_OID_HEX_ZERO, hex, spec->rref);
				}
			}
		} else {
			/* delete remote reference */
			git_vector_foreach(&push->remote->refs, j, head) {
				if (!strcmp(spec->rref, head->name)) {
					len += strlen(spec->rref);

					git_oid_fmt(hex, &head->oid);
					git_buf_printf(buf, "%04x%s %s %s\n", len,
						       hex, GIT_OID_HEX_ZERO, head->name);

					break;
				}
			}
		}
	}
	git_buf_puts(buf, "0000");
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

		if (!git_oid_cmp(&spec->loid, &spec->roid))
			continue; /* up-to-date */

		if (git_revwalk_push(rw, &spec->loid) < 0)
			goto on_error;

		if (!spec->force) {
			; /* TODO: check if common ancestor */
		}

		if (!git_oid_iszero(&spec->roid)) {
			if (git_revwalk_hide(rw, &spec->roid) < 0)
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
	return error == GIT_REVWALKOVER ? 0 : error;
}

static int queue_objects(git_push *push)
{
	git_vector commits;
	git_oid *o;
	unsigned int i;
	int error = -1;

	if (git_vector_init(&commits, 0, NULL) < 0)
		return -1;

	if (revwalk(&commits, push) < 0)
		goto on_error;

	if (!commits.length)
		return 0; /* nothing to do */

	git_vector_foreach(&commits, i, o) {
		if (git_packbuilder_insert(push->pb, o, NULL) < 0)
			goto on_error;
	}

	git_vector_foreach(&commits, i, o) {
		git_object *obj;

		if (git_object_lookup(&obj, push->repo, o, GIT_OBJ_ANY) < 0)
			goto on_error;

		switch (git_object_type(obj)) {
		case GIT_OBJ_TAG: /* TODO: expect tags */
		case GIT_OBJ_COMMIT:
			if (git_packbuilder_insert_tree(push->pb,
					git_commit_tree_oid((git_commit *)obj)) < 0) {
				git_object_free(obj);
				goto on_error;
			}
			break;
		case GIT_OBJ_TREE:
		case GIT_OBJ_BLOB:
		default:
			git_object_free(obj);
			giterr_set(GITERR_INVALID, "Given object type invalid");
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

static int do_push(git_push *push)
{
	git_transport *t = push->remote->transport;
	git_buf pktline = GIT_BUF_INIT;

	if (gen_pktline(&pktline, push) < 0)
		goto on_error;

#ifdef PUSH_DEBUG
{
	git_remote_head *head;
	push_spec *spec;
	unsigned int i;
	char hex[41]; hex[40] = '\0';

	git_vector_foreach(&push->remote->refs, i, head) {
		git_oid_fmt(hex, &head->oid);
		fprintf(stderr, "%s (%s)\n", hex, head->name);
	}

	git_vector_foreach(&push->specs, i, spec) {
		git_oid_fmt(hex, &spec->roid);
		fprintf(stderr, "%s (%s) -> ", hex, spec->lref);
		git_oid_fmt(hex, &spec->loid);
		fprintf(stderr, "%s (%s)\n", hex, spec->rref ?
			spec->rref : spec->lref);
	}
}
#endif

	if (git_packbuilder_new(&push->pb, push->repo) < 0)
		goto on_error;

	if (queue_objects(push) < 0)
		goto on_error;

	if (t->rpc) {
		git_buf pack = GIT_BUF_INIT;

		if (git_packbuilder_write_buf(&pack, push->pb) < 0)
			goto on_error;

		if (t->push(t, &pktline, &pack) < 0) {
			git_buf_free(&pack);
			goto on_error;
		}

		git_buf_free(&pack);
	} else {
		if (gitno_send(push->remote->transport,
			       pktline.ptr, pktline.size, 0) < 0)
			goto on_error;

		if (git_packbuilder_send(push->pb, push->remote->transport) < 0)
			goto on_error;
	}

	git_packbuilder_free(push->pb);
	git_buf_free(&pktline);
	return 0;

on_error:
	git_packbuilder_free(push->pb);
	git_buf_free(&pktline);
	return -1;
}

static int cb_filter_refs(git_remote_head *ref, void *data)
{
	git_remote *remote = data;
	return git_vector_insert(&remote->refs, ref);
}

static int filter_refs(git_remote *remote)
{
	git_vector_clear(&remote->refs);
	return git_remote_ls(remote, cb_filter_refs, remote);
}

int git_push_finish(git_push *push)
{
	if (!git_remote_connected(push->remote)) {
		if (git_remote_connect(push->remote, GIT_DIR_PUSH) < 0)
			return -1;
	}

	if (filter_refs(push->remote) < 0 || do_push(push) < 0) {
		git_remote_disconnect(push->remote);
		return -1;
	}

	git_remote_disconnect(push->remote);
	return 0;
}

void git_push_free(git_push *push)
{
	push_spec *spec;
	unsigned int i;

	if (push == NULL)
		return;

	git_vector_foreach(&push->specs, i, spec) {
		free_refspec(spec);
	}
	git_vector_free(&push->specs);

	git__free(push);
}
