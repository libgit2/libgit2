/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "smart.h"

#include "git2.h"
#include "git2/sys/remote.h"
#include "refs.h"
#include "refspec.h"
#include "proxy.h"

static int ref_name_cmp(const void *a, const void *b)
{
	const git_pkt_ref *ref_a = a, *ref_b = b;

	return strcmp(ref_a->head.name, ref_b->head.name);
}

int git_transport_smart__session_init(
	git_transport_smart_session *s,
	git_remote *owner,
	bool stateless)
{
	s->owner = owner;
	s->stateless = stateless;

	if (git_vector_init(&s->refs, 16, ref_name_cmp) < 0 ||
	    git_vector_init(&s->heads, 16, ref_name_cmp) < 0)
		return -1;

	git_staticstr_init(&s->buffer, GIT_TRANSPORT_SMART_BUFFER_SIZE);

	return 0;
}

void git_transport_smart_session_reset(git_transport_smart_session *s)
{
	git__free(s->caps.object_format);
	s->caps.object_format = NULL;

	git__free(s->caps.agent);
	s->caps.agent = NULL;
}

void git_transport_smart_session_close(git_transport_smart_session *s)
{
	unsigned int i;
	git_pkt *p;

	git_transport_smart_session_reset(s);

	git_vector_foreach(&s->common, i, p)
		git_pkt_free(p);

	git_vector_dispose(&s->common);

	git__free(s->url);
	s->url = NULL;
}

void git_transport_smart__session_dispose(git_transport_smart_session *s)
{
	unsigned int i;
	git_pkt *p;

	git_transport_smart_session_close(s);

	git_vector_dispose(&s->heads);

	git_vector_foreach(&s->refs, i, p)
		git_pkt_free(p);

	git_vector_dispose(&s->refs);

	git_remote_connect_options_dispose(&s->connect_opts);

	git_array_dispose(s->shallow_roots);
}

int git_transport_smart__recv(git_transport_smart_session *s)
{
	size_t bytes_read;
	int ret;

	GIT_ASSERT_ARG(s);
	GIT_ASSERT(s->stream);

	if (git_staticstr_remain(&s->buffer) == 0) {
		git_error_set(GIT_ERROR_NET, "out of buffer space");
		return -1;
	}

	ret = s->stream->read(s->stream,
		git_staticstr_offset(&s->buffer),
		git_staticstr_remain(&s->buffer),
		&bytes_read);

	if (ret < 0)
		return ret;

	GIT_ASSERT(bytes_read <= INT_MAX);
	GIT_ASSERT(bytes_read <= git_staticstr_remain(&s->buffer));

	git_staticstr_increase(&s->buffer, bytes_read);

	if (s->packetsize_cb && !s->cancelled.val) {
		ret = s->packetsize_cb(bytes_read, s->packetsize_payload);

		if (ret) {
			git_atomic32_set(&s->cancelled, 1);
			return GIT_EUSER;
		}
	}

	return (int)bytes_read;
}

int git_transport_smart__update_heads(git_transport_smart_session *s, git_vector *symrefs)
{
	size_t i;
	git_pkt *pkt;

	git_vector_clear(&s->heads);
	git_vector_foreach(&s->refs, i, pkt) {
		git_pkt_ref *ref = (git_pkt_ref *) pkt;
		if (pkt->type != GIT_PKT_REF)
			continue;

		if (symrefs) {
			git_refspec *spec;
			git_str buf = GIT_STR_INIT;
			size_t j;
			int error = 0;

			git_vector_foreach(symrefs, j, spec) {
				git_str_clear(&buf);
				if (git_refspec_src_matches(spec, ref->head.name) &&
				    !(error = git_refspec__transform(&buf, spec, ref->head.name))) {
					git__free(ref->head.symref_target);
					ref->head.symref_target = git_str_detach(&buf);
				}
			}

			git_str_dispose(&buf);

			if (error < 0)
				return error;
		}

		if (git_vector_insert(&s->heads, &ref->head) < 0)
			return -1;
	}

	return 0;
}

static void free_symrefs(git_vector *symrefs)
{
	git_refspec *spec;
	size_t i;

	git_vector_foreach(symrefs, i, spec) {
		git_refspec__dispose(spec);
		git__free(spec);
	}

	git_vector_dispose(symrefs);
}

int git_transport_smart_read_advertisement(
	git_transport_smart_session *s,
	git_transport_smart_stream *stream)
{
	git_pkt *pkt;
	git_pkt_ref *first;
	git_vector symrefs;
	int error;

	s->stream = stream;

	/* 2 flushes for RPC; 1 for stateful */
	if ((error = git_transport_smart__store_refs(s, s->stateless ? 2 : 1)) < 0)
		return error;

	/* Strip the comment packet for RPC */
	if (s->stateless) {
		pkt = (git_pkt *)git_vector_get(&s->refs, 0);

		if (!pkt || GIT_PKT_COMMENT != pkt->type) {
			git_error_set(GIT_ERROR_NET, "invalid response");
			return -1;
		} else {
			/* Remove the comment pkt from the list */
			git_vector_remove(&s->refs, 0);
			git__free(pkt);
		}
	}

	/* We now have loaded the refs. */
	s->have_refs = 1;

	pkt = (git_pkt *)git_vector_get(&s->refs, 0);
	if (pkt && GIT_PKT_REF != pkt->type) {
		git_error_set(GIT_ERROR_NET, "invalid response");
		return -1;
	}
	first = (git_pkt_ref *)pkt;

	if ((error = git_vector_init(&symrefs, 1, NULL)) < 0)
		return error;

	/* Detect capabilities */
	if ((error = git_transport_smart__detect_caps(first, &s->caps, &symrefs)) == 0) {
		/* If the only ref in the list is capabilities^{} with OID_ZERO, remove it */
		if (1 == s->refs.length && !strcmp(first->head.name, "capabilities^{}") &&
			git_oid_is_zero(&first->head.oid)) {
			git_vector_clear(&s->refs);
			git_pkt_free((git_pkt *)first);
		}

		/* Keep a list of heads for _ls */
		git_transport_smart__update_heads(s, &symrefs);
	} else if (error == GIT_ENOTFOUND) {
		/* There was no ref packet received, or the cap list was empty */
		error = 0;
	} else {
		git_error_set(GIT_ERROR_NET, "invalid response");
	}

	free_symrefs(&symrefs);

	return error;
}

void git_transport_smart_goodbye(git_transport_smart_stream *stream)
{
	const char flush[] = "0000";

	stream->write(stream, flush, 4);
}

static int git_transport_smart__set_connect_opts(
	git_transport *transport,
	const git_remote_connect_options *opts)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	if (!t->session->connected) {
		git_error_set(GIT_ERROR_NET, "cannot reconfigure a transport that is not connected");
		return -1;
	}

	return git_remote_connect_options_normalize(&t->session->connect_opts, t->session->owner->repo, opts);
}

static int git_transport_smart__capabilities(unsigned int *capabilities, git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	*capabilities = 0;

	if (t->session->caps.push_options)
		*capabilities |= GIT_REMOTE_CAPABILITY_PUSH_OPTIONS;

	if (t->session->caps.want_tip_sha1)
		*capabilities |= GIT_REMOTE_CAPABILITY_TIP_OID;

	if (t->session->caps.want_reachable_sha1)
		*capabilities |= GIT_REMOTE_CAPABILITY_REACHABLE_OID;

	return 0;
}

static int git_transport_smart__oid_type(git_oid_t *out, git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	*out = 0;

	if (t->session->caps.object_format == NULL) {
		*out = GIT_OID_DEFAULT;
	} else {
		*out = git_oid_type_fromstr(t->session->caps.object_format);

		if (!*out) {
			git_error_set(GIT_ERROR_INVALID,
				"unknown object format '%s'",
				t->session->caps.object_format);
			return -1;
		}
	}

	return 0;
}

static int git_transport_smart__ls(const git_remote_head ***out, size_t *size, git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	if (!t->session->have_refs) {
		git_error_set(GIT_ERROR_NET, "the transport has not yet loaded the refs");
		return -1;
	}

	*out = (const git_remote_head **) t->session->heads.contents;
	*size = t->session->heads.length;

	return 0;
}

static int git_transport_smart_base__shallow_roots(git_oidarray *out, git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	return git_transport_smart__shallow_roots(out, t->session);
}

static void git_transport_smart__cancel(git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	git_atomic32_set(&t->session->cancelled, 1);
}

static int git_transport_smart__is_connected(git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	return t->session->connected;
}

int git_transport_smart_base_init(
	git_transport_smart_base *t,
	git_remote *owner,
	int stateless)
{
	t->parent.version = GIT_TRANSPORT_VERSION;
	t->parent.set_connect_opts = git_transport_smart__set_connect_opts;
	t->parent.capabilities = git_transport_smart__capabilities;
	t->parent.oid_type = git_transport_smart__oid_type;
	t->parent.ls = git_transport_smart__ls;
	t->parent.shallow_roots = git_transport_smart_base__shallow_roots;
	t->parent.is_connected = git_transport_smart__is_connected;
	t->parent.cancel = git_transport_smart__cancel;

	t->session = git__calloc(1, sizeof(git_transport_smart_session));
	GIT_ERROR_CHECK_ALLOC(t->session);

	return git_transport_smart__session_init(t->session, owner, stateless);
}

void git_transport_smart_base_dispose(git_transport_smart_base *t)
{
	if (!t->session)
		return;

	git_transport_smart__session_dispose(t->session);

	git__free(t->session);
	t->session = NULL;
}

int git_transport_smart_base_prepare_connect(
	git_transport_smart_base *t,
	const char *url,
	int direction,
	const git_remote_connect_options *connect_opts)
{
	if (git_remote_connect_options_normalize(&t->session->connect_opts, t->session->owner->repo, connect_opts) < 0)
		return -1;

	git__free(t->session->url);

	t->session->url = git__strdup(url);
	GIT_ERROR_CHECK_ALLOC(t->session->url);

	t->session->direction = direction;

	return 0;
}

const char *git_transport_smart_base_url(git_transport_smart_base *t)
{
	return t->session->url;
}

void git_transport_smart_base_set_connected(
	git_transport_smart_base *t,
	int connected)
{
	t->session->connected = connected;
}

int git_transport_smart_base_ensure_direction(
	git_transport_smart_base *t,
	int direction)
{
	if (t->session->direction != direction) {
		git_error_set(GIT_ERROR_NET, "this operation is only valid for %s",
			direction == GIT_DIRECTION_FETCH ? "fetch" : "push");
		return -1;
	}

	return 0;
}

int git_transport_smart_certificate_check(git_transport *transport, git_cert *cert, int valid, const char *hostname)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);
	git_remote_connect_options *connect_opts = &t->session->connect_opts;

	GIT_ASSERT_ARG(transport);
	GIT_ASSERT_ARG(cert);
	GIT_ASSERT_ARG(hostname);

	if (!connect_opts->callbacks.certificate_check)
		return GIT_PASSTHROUGH;

	return connect_opts->callbacks.certificate_check(cert, valid, hostname, connect_opts->callbacks.payload);
}

int git_transport_smart_credentials(git_credential **out, git_transport *transport, const char *user, int methods)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);
	git_remote_connect_options *connect_opts = &t->session->connect_opts;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(transport);

	if (!connect_opts->callbacks.credentials)
		return GIT_PASSTHROUGH;

	return connect_opts->callbacks.credentials(out, t->session->url, user, methods, connect_opts->callbacks.payload);
}

int git_transport_remote_connect_options(
		git_remote_connect_options *out,
		git_transport *transport)
{
	git_transport_smart_base *t = GIT_CONTAINER_OF(transport, git_transport_smart_base, parent);

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(transport);

	return git_remote_connect_options_dup(out, &t->session->connect_opts);
}
