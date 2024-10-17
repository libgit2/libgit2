/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "git2.h"
#include "git2/odb_backend.h"

#include "smart.h"
#include "refs.h"
#include "repository.h"
#include "push.h"
#include "pack-objects.h"
#include "remote.h"
#include "util.h"
#include "revwalk.h"

#define NETWORK_XFER_THRESHOLD (100*1024)
/* The minimal interval between progress updates (in seconds). */
#define MIN_PROGRESS_UPDATE_INTERVAL 0.5

bool git_smart__ofs_delta_enabled = true;


/*
 * Flags used to mark commits during negotiation.
 */

/**
 * Commit is a common reference, determined by comparing remote and local heads
 * before starting graph traversal.
 *
 * We need to tell apart between `COMMIT_FLAG_COMMON` and `COMMIT_FLAG_COMMON_REF`
 * because we want to mark parents of both as common, but we want to generate
 * a "have" statement for a common reference that is not a known common commit.
 */
#define COMMIT_FLAG_COMMON_REF (uintptr_t)(1 << 0)

/**
 * A commit is known to be common between client and server.
 *
 * This can be set as consequence of the server directly acknowledging a commit
 * as common, or by marking parents of such a commit, or a common reference.
 */
#define COMMIT_FLAG_COMMON (uintptr_t)(1 << 1)

/**
 * A commit is a local reference.
 *
 * Used only during the initial phase to match local and remote references.
 */
#define COMMIT_FLAG_LOCAL_REF (uintptr_t)(1 << 2)

/**
 * A commit is currently counted as being not common.
 *
 * A stop condition for the negotiation is running out of non-common queued
 * commits. To track that, we keep a counter for that. However, commits can be
 * determined to be common in multiple places, and to make sure we don't
 * decrement twice, we use a bit to mark them.
 */
#define COMMIT_FLAG_COUNTED_AS_UNCOMMON (uintptr_t)(1 << 3)


/**
 * These 3 constants control the max number of "have" statements sent by each
 * step of negotiation.
 */
#define HAVE_STATEMENTS_INITIAL 16
#define HAVE_STATEMENTS_NON_RPC_THRESHOLD 32
#define HAVE_STATEMENTS_RPC_THRESHOLD 16384


/**
 * Threshold for stopping traversing if no common commit was found, but at least
 * a common commit is known from a previous step.
 */
#define MAX_TRIES_WITHOUT_HAVE_STATEMENT 256


/**
 * Flags used during traversal.
 *
 * Using `uintptr_t` since they are stored directly in a `(void *)`.
 */
typedef uintptr_t negotiation_commit_flags;


/**
 * Result returned from `process_packets`.
 */
typedef struct ack_pkts_processing_result {

	/**
	 * A "ready" ACK packet was received.
	 */
	unsigned received_ready: 1,

		/**
		 * Received at least one ACK packet other that "common".
		 */
		received_other_than_ack_common: 1,

		/**
		 * Received at least one ACK packet with subtype different than `GIT_ACK_NONE`.
		 */
		received_specific_ack: 1,

		/**
		 * (For RPC only) At least one "have" statement has been written on the
		 * buffer for the next negotiation step.
		 */
		have_statement_written: 1;
} ack_pkts_processing_result;

#define ACK_PKTS_PROCESSING_RESULT_INIT {0, 0, 0, 0}




int git_smart__store_refs(transport_smart *t, int flushes)
{
	git_vector *refs = &t->refs;
	int error, flush = 0, recvd;
	const char *line_end = NULL;
	git_pkt *pkt = NULL;
	git_pkt_parse_data pkt_parse_data = { 0 };
	size_t i;

	/* Clear existing refs in case git_remote_connect() is called again
	 * after git_remote_disconnect().
	 */
	git_vector_foreach(refs, i, pkt) {
		git_pkt_free(pkt);
	}
	git_vector_clear(refs);
	pkt = NULL;

	do {
		if (t->buffer.len > 0)
			error = git_pkt_parse_line(&pkt, &line_end,
				t->buffer.data, t->buffer.len,
				&pkt_parse_data);
		else
			error = GIT_EBUFS;

		if (error < 0 && error != GIT_EBUFS)
			return error;

		if (error == GIT_EBUFS) {
			if ((recvd = git_smart__recv(t)) < 0)
				return recvd;

			if (recvd == 0) {
				git_error_set(GIT_ERROR_NET, "could not read refs from remote repository");
				return GIT_EEOF;
			}

			continue;
		}

		git_staticstr_consume(&t->buffer, line_end);

		if (pkt->type == GIT_PKT_ERR) {
			git_error_set(GIT_ERROR_NET, "remote error: %s", ((git_pkt_err *)pkt)->error);
			git__free(pkt);
			return -1;
		}

		if (pkt->type != GIT_PKT_FLUSH && git_vector_insert(refs, pkt) < 0)
			return -1;

		if (pkt->type == GIT_PKT_FLUSH) {
			flush++;
			git_pkt_free(pkt);
		}
	} while (flush < flushes);

	return flush;
}

static int append_symref(const char **out, git_vector *symrefs, const char *ptr)
{
	int error;
	const char *end;
	git_str buf = GIT_STR_INIT;
	git_refspec *mapping = NULL;

	ptr += strlen(GIT_CAP_SYMREF);
	if (*ptr != '=')
		goto on_invalid;

	ptr++;
	if (!(end = strchr(ptr, ' ')) &&
	    !(end = strchr(ptr, '\0')))
		goto on_invalid;

	if ((error = git_str_put(&buf, ptr, end - ptr)) < 0)
		return error;

	/* symref mapping has refspec format */
	mapping = git__calloc(1, sizeof(git_refspec));
	GIT_ERROR_CHECK_ALLOC(mapping);

	error = git_refspec__parse(mapping, git_str_cstr(&buf), true);
	git_str_dispose(&buf);

	/* if the error isn't OOM, then it's a parse error; let's use a nicer message */
	if (error < 0) {
		if (git_error_last()->klass != GIT_ERROR_NOMEMORY)
			goto on_invalid;

		git__free(mapping);
		return error;
	}

	if ((error = git_vector_insert(symrefs, mapping)) < 0)
		return error;

	*out = end;
	return 0;

on_invalid:
	git_error_set(GIT_ERROR_NET, "remote sent invalid symref");
	git_refspec__dispose(mapping);
	git__free(mapping);
	return -1;
}

int git_smart__detect_caps(
	git_pkt_ref *pkt,
	transport_smart_caps *caps,
	git_vector *symrefs)
{
	const char *ptr, *start;

	/* No refs or capabilities, odd but not a problem */
	if (pkt == NULL || pkt->capabilities == NULL)
		return GIT_ENOTFOUND;

	ptr = pkt->capabilities;
	while (ptr != NULL && *ptr != '\0') {
		if (*ptr == ' ')
			ptr++;

		if (git_smart__ofs_delta_enabled && !git__prefixcmp(ptr, GIT_CAP_OFS_DELTA)) {
			caps->common = caps->ofs_delta = 1;
			ptr += strlen(GIT_CAP_OFS_DELTA);
			continue;
		}

		/* Keep multi_ack_detailed before multi_ack */
		if (!git__prefixcmp(ptr, GIT_CAP_MULTI_ACK_DETAILED)) {
			caps->common = caps->multi_ack_detailed = 1;
			ptr += strlen(GIT_CAP_MULTI_ACK_DETAILED);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_MULTI_ACK)) {
			caps->common = caps->multi_ack = 1;
			ptr += strlen(GIT_CAP_MULTI_ACK);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_INCLUDE_TAG)) {
			caps->common = caps->include_tag = 1;
			ptr += strlen(GIT_CAP_INCLUDE_TAG);
			continue;
		}

		/* Keep side-band check after side-band-64k */
		if (!git__prefixcmp(ptr, GIT_CAP_SIDE_BAND_64K)) {
			caps->common = caps->side_band_64k = 1;
			ptr += strlen(GIT_CAP_SIDE_BAND_64K);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_SIDE_BAND)) {
			caps->common = caps->side_band = 1;
			ptr += strlen(GIT_CAP_SIDE_BAND);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_DELETE_REFS)) {
			caps->common = caps->delete_refs = 1;
			ptr += strlen(GIT_CAP_DELETE_REFS);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_PUSH_OPTIONS)) {
			caps->common = caps->push_options = 1;
			ptr += strlen(GIT_CAP_PUSH_OPTIONS);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_THIN_PACK)) {
			caps->common = caps->thin_pack = 1;
			ptr += strlen(GIT_CAP_THIN_PACK);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_SYMREF)) {
			int error;

			if ((error = append_symref(&ptr, symrefs, ptr)) < 0)
				return error;

			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_WANT_TIP_SHA1)) {
			caps->common = caps->want_tip_sha1 = 1;
			ptr += strlen(GIT_CAP_WANT_TIP_SHA1);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_WANT_REACHABLE_SHA1)) {
			caps->common = caps->want_reachable_sha1 = 1;
			ptr += strlen(GIT_CAP_WANT_REACHABLE_SHA1);
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_OBJECT_FORMAT)) {
			ptr += strlen(GIT_CAP_OBJECT_FORMAT);

			start = ptr;
			ptr = strchr(ptr, ' ');

			if ((caps->object_format = git__strndup(start, (ptr - start))) == NULL)
				return -1;
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_AGENT)) {
			ptr += strlen(GIT_CAP_AGENT);

			start = ptr;
			ptr = strchr(ptr, ' ');

			if ((caps->agent = git__strndup(start, (ptr - start))) == NULL)
				return -1;
			continue;
		}

		if (!git__prefixcmp(ptr, GIT_CAP_SHALLOW)) {
			caps->common = caps->shallow = 1;
			ptr += strlen(GIT_CAP_SHALLOW);
			continue;
		}

		/* We don't know this capability, so skip it */
		ptr = strchr(ptr, ' ');
	}

	return 0;
}

static int recv_pkt(
	git_pkt **out_pkt,
	git_pkt_type *out_type,
	transport_smart *t)
{
	const char *ptr = t->buffer.data, *line_end = ptr;
	git_pkt *pkt = NULL;
	git_pkt_parse_data pkt_parse_data = { 0 };
	int error = 0, ret;

	pkt_parse_data.oid_type = t->owner->repo->oid_type;
	pkt_parse_data.seen_capabilities = 1;

	do {
		if (t->buffer.len > 0)
			error = git_pkt_parse_line(&pkt, &line_end, ptr,
				t->buffer.len, &pkt_parse_data);
		else
			error = GIT_EBUFS;

		if (error == 0)
			break; /* return the pkt */

		if (error < 0 && error != GIT_EBUFS)
			return error;

		if ((ret = git_smart__recv(t)) < 0) {
			return ret;
		} else if (ret == 0) {
			git_error_set(GIT_ERROR_NET, "could not read from remote repository");
			return GIT_EEOF;
		}
	} while (error);

	git_staticstr_consume(&t->buffer, line_end);

	if (out_type != NULL)
		*out_type = pkt->type;
	if (out_pkt != NULL)
		*out_pkt = pkt;
	else
		git__free(pkt);

	return error;
}

/**
 * Marks a commit's parents recursively, and optionally the commit itself, as common.
 *
 * Note the parents wont be marked if the walker's `git_commit_list_node` for
 * the given OID hasn't been parsed yet (otherwise, this function would recurse
 * the entire graph until reaching the root).
 *
 * Since walking is made with a callback that hides all common commits, this is
 * enough as the parents we missed wont be traversed at all unless another path
 * coming from non-common commits happens to walk them. This is rare and worst
 * case it adds a few commits that will eventually be marked as common as well.
 *
 * @param oid The OID of the base commit for marking.
 * @param mark_parents_only If not 0, `oid` wont be marked, only the parents.
 * @param marked_oids The map of marked OIDs used during negotiation.
 * @param walk The revwalker used during negotiation.
 * @param non_common_queued_commits Pointer to the non common commit count.
 */
static void mark_as_common(
	const git_oid *oid,
	int mark_parents_only,
	git_oidmap * const marked_oids,
	git_revwalk * const walk,
	size_t * const non_common_queued_commits)
{
	negotiation_commit_flags flags;
	flags = (negotiation_commit_flags) git_oidmap_get(marked_oids, oid);

    if (!mark_parents_only && flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON) {
		(*non_common_queued_commits)--;
		flags &= ~COMMIT_FLAG_COUNTED_AS_UNCOMMON;
		git_oidmap_set(marked_oids, oid, (void*) flags);
	}

	if (!(flags & COMMIT_FLAG_COMMON)) {
		uint16_t i;
		git_commit_list_node *node = git_oidmap_get(walk->commits, oid);
		int node_ready = node != NULL && node->parsed;

		if (!mark_parents_only) {
			flags |= COMMIT_FLAG_COMMON;
			git_oidmap_set(marked_oids, oid, (void*) flags);
		}

		if (node_ready) {
			for (i = 0; i < node->out_degree; i++) {
				git_commit_list_node *p = node->parents[i];
				mark_as_common(&p->oid, 0, marked_oids, walk, non_common_queued_commits);
			}
		}
	}
}

/**
 * When negotiating using `multi_ack` or `multi_ack_detailed`, processes the
 * ACK packets returned by the server during a negotiation step.
 *
 * The appropriate "have" statements will be written to `data` for the next
 * negotiation step. An "have" statement will be written for each commit
 * acknowledged as `GIT_ACK_COMMON` that wasn't already known to be common
 * before this negotiation step.
 *
 * @param out See `ack_pkts_processing_result` documentation for details.
 * @param t The smart transport used for the negotiation.
 * @param marked_oids The map of marked OIDs used during negotiation.
 * @param data A data buffer with "want" statements, ready for receiving new
 * "have" statements for the next negotiation step.
 * @param walk The revwalker used during negotiation.
 * @param non_common_queued_commits Pointer to the non common commit count.
 * @return 0 or an error code.
 */
static int process_packets(
	ack_pkts_processing_result *out,
	transport_smart * const t,
	git_oidmap * const marked_oids,
	git_str * const data,
	git_revwalk * const walk,
    size_t * const non_common_queued_commits)
{
	git_pkt *pkt = NULL;
	git_pkt_ack *pkt_ack;
	negotiation_commit_flags flags;
	int error;

	do {
		if ((error = recv_pkt(&pkt, NULL, t)) < 0)
			return error;

		if (pkt->type != GIT_PKT_ACK) {
			git__free(pkt);
			return 0;
		}

		pkt_ack = (git_pkt_ack*) pkt;
		flags = (negotiation_commit_flags) git_oidmap_get(marked_oids, &pkt_ack->oid);

		mark_as_common(&pkt_ack->oid, 1, marked_oids, walk, non_common_queued_commits);

		if (!(flags & COMMIT_FLAG_COMMON) && pkt_ack->status == GIT_ACK_COMMON) {
			/*
			 * It's OK to free here because `mark_as_common` was called for marking
			 * parents only, therefore the OID does not end being referred by
			 * `marked_oids`.
			 */
			if (git_vector_insert(&t->common, pkt) < 0) {
				git__free(pkt);
				return -1;
			}

			if (t->rpc) {
				out->have_statement_written = 1;
				if ((error = git_pkt_buffer_have(&pkt_ack->oid, data)) < 0)
					return -1;

				if (git_str_oom(data)) {
					return -1;
				}
			}
		}

		switch (pkt_ack->status) {
			case GIT_ACK_READY:
				out->received_ready = 1;
				/* fall through */

			case GIT_ACK_CONTINUE:
				out->received_other_than_ack_common = 1;
				/* fall through */
				
			case GIT_ACK_COMMON:
				out->received_specific_ack = 1;
				break;

			default:
				break;
		}
	} while (1);

	return 0;
}

static int wait_while_ack(transport_smart *t)
{
	int error;
	git_pkt *pkt = NULL;
	git_pkt_ack *ack = NULL;

	while (1) {
		git_pkt_free(pkt);

		if ((error = recv_pkt(&pkt, NULL, t)) < 0)
			return error;

		if (pkt->type == GIT_PKT_NAK)
			break;
		if (pkt->type != GIT_PKT_ACK)
			continue;

		ack = (git_pkt_ack*)pkt;

		if (ack->status != GIT_ACK_CONTINUE &&
		    ack->status != GIT_ACK_COMMON &&
		    ack->status != GIT_ACK_READY) {
			break;
		}
	}

	git_pkt_free(pkt);
	return 0;
}

/**
 * Returns the total "have" statement count when the buffer should be flushed
 * and a new negotiation step performed.
 *
 * @param transport The smart transport used for the negotiation.
 * @param count The current total number of "have" statements sent to the server
 * in the multiple negotiation steps performed so far.
 * @return The next count when a new negotiation step should occur.
 */
static int next_flush(transport_smart *transport, int count) {
	if (transport->rpc) {
		return (count < HAVE_STATEMENTS_RPC_THRESHOLD) ? count * 2 : count * 11 / 10;
	} else {
		return (count < HAVE_STATEMENTS_NON_RPC_THRESHOLD) ? count * 2 : count + HAVE_STATEMENTS_NON_RPC_THRESHOLD;
	}
}

/**
 * Callback that hides any common commit.
 */
static int negotiation_hide_cb(const git_oid *commit_id, void *payload) {
	git_oidmap *common_oids = (git_oidmap*) payload;
	negotiation_commit_flags flags = (negotiation_commit_flags) git_oidmap_get(common_oids, commit_id);

	return !!(flags & COMMIT_FLAG_COMMON);
}

static int cap_not_sup_err(const char *cap_name)
{
	git_error_set(GIT_ERROR_NET, "server doesn't support %s", cap_name);
	return GIT_EINVALID;
}

/* Disables server capabilities we're not interested in */
static int setup_caps(
	transport_smart_caps *caps,
	const git_fetch_negotiation *wants)
{
	if (wants->depth > 0) {
		if (!caps->shallow)
			return cap_not_sup_err(GIT_CAP_SHALLOW);
	} else {
		caps->shallow = 0;
	}

	return 0;
}

static int setup_shallow_roots(
	git_array_oid_t *out,
	const git_fetch_negotiation *wants)
{
	git_array_clear(*out);

	if (wants->shallow_roots_len > 0) {
		git_array_init_to_size(*out, wants->shallow_roots_len);
		GIT_ERROR_CHECK_ALLOC(out->ptr);

		memcpy(out->ptr, wants->shallow_roots,
		       sizeof(git_oid) * wants->shallow_roots_len);
		out->size = wants->shallow_roots_len;
	}

	return 0;
}


//int git_smart__negotiate_fetch(git_transport *transport, git_repository *repo, const git_remote_head * const *wants, size_t count)

int git_smart__negotiate_fetch(
	git_transport *transport,
	git_repository *repo,
	const git_fetch_negotiation *wants)
{
	transport_smart *t = (transport_smart *)transport;
	git_revwalk__push_options opts = GIT_REVWALK__PUSH_OPTIONS_INIT;
	git_str data = GIT_STR_INIT;
	git_revwalk *walk = NULL;
	int error = -1;
	git_pkt_type pkt_type;
	unsigned int i = 0;
    git_oid oid;
	git_oidmap *common_oids = NULL;
	git_remote_head *head;
	git_commit_list *list;
	size_t c;
	uint16_t p;
    negotiation_commit_flags flags;
    size_t non_common_queued_commits = 0;
	git_commit_list_node *node;
	unsigned int flush_limit = HAVE_STATEMENTS_INITIAL;
	unsigned int tries = 0;
    int received_specific_ack = 0;

	if ((error = setup_caps(&t->caps, wants)) < 0 ||
	    (error = setup_shallow_roots(&t->shallow_roots, wants)) < 0)
		return error;

	if ((error = git_pkt_buffer_wants(wants, &t->caps, &data)) < 0)
		return error;

	if ((error = git_oidmap_new(&common_oids)) < 0)
		goto on_error;

	if ((error = git_revwalk_new(&walk, repo)) < 0)
		goto on_error;

	if ((error = git_revwalk_add_hide_cb(walk, negotiation_hide_cb, common_oids)) < 0)
		goto on_error;

	opts.insert_by_date = 1;
	if ((error = git_revwalk__push_glob(walk, "refs/*", &opts)) < 0)
		goto on_error;

	if (wants->depth > 0) {
		git_pkt_shallow *pkt;

		if ((error = git_smart__negotiation_step(&t->parent, data.ptr, data.size)) < 0)
			goto on_error;

		while ((error = recv_pkt((git_pkt **)&pkt, NULL, t)) == 0) {
			bool complete = false;

			if (pkt->type == GIT_PKT_SHALLOW) {
				error = git_oidarray__add(&t->shallow_roots, &pkt->oid);
			} else if (pkt->type == GIT_PKT_UNSHALLOW) {
				git_oidarray__remove(&t->shallow_roots, &pkt->oid);
			} else if (pkt->type == GIT_PKT_FLUSH) {
				/* Server is done, stop processing shallow oids */
				complete = true;
			} else {
				git_error_set(GIT_ERROR_NET, "unexpected packet type");
				error = -1;
			}

			git_pkt_free((git_pkt *) pkt);

			if (complete || error < 0)
				break;
		}

		if (error < 0)
			goto on_error;
	}

	/*
	 * Let's start by poking into the revwalk and grab all the client tips added
	 * by git_revwalk__push_glob, and store the OIDs fagged as tips.
	 */
	for (list = walk->user_input; list != NULL; list = list->next) {
		flags = (negotiation_commit_flags)git_oidmap_get(common_oids, &list->item->oid);

		if (!(flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON))
			non_common_queued_commits++;

		git_oidmap_set(common_oids, &list->item->oid,
			(void*) ((COMMIT_FLAG_LOCAL_REF) | COMMIT_FLAG_COUNTED_AS_UNCOMMON));
	}

	/*
	 * Now, for each remote head that points to the same OID as a tip, mark it
	 * as common. It doesn't really matter if both references were pointing to
	 * the same reference or not. We only want matching OIDs, whatever the
	 * references may be. We know we will walk all the tips anyway.
	 */
	git_vector_foreach(&t->heads, c, head) {
		flags = (negotiation_commit_flags)git_oidmap_get(common_oids, &head->oid);

        if (flags & COMMIT_FLAG_LOCAL_REF) {
			if (flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON)
				non_common_queued_commits--;

			/*
			 * Note: When re-connect is implemented to restart a connection if
			 * dropped by a timeout, make sure t->heads remains retained, otherwise
			 * the head->oid key used by the set will be deallocated as well.
			 */
            git_oidmap_set(common_oids, &head->oid, (void*) ((flags & ~COMMIT_FLAG_COUNTED_AS_UNCOMMON) | COMMIT_FLAG_COMMON_REF));
		}
	}

	while (1) {
        if (non_common_queued_commits == 0)
            break;
        
        error = git_revwalk_next(&oid, walk);

		/*
		 * Note: From here on, we know a commit is not COMMIT_FLAG_COMMON otherwise
		 * it would have been excluded by the revwalk callback.
		 */

		if (error < 0) {
			if (GIT_ITEROVER == error)
				break;

			goto on_error;
		}

		tries++;
        flags = (negotiation_commit_flags)git_oidmap_get(common_oids, &oid);

		/*
		 * There are two reasons we poke into the revwalk and get the commit instead
		 * parsing.
		 *
		 * 1. Speed. Parsing is slow, and at this point we know the walker already
		 * parsed. So leverage that work.
		 *
		 * 2. We need the OID to be retained to be used as a key for the oid map.
		 * If we parse the commit and mark parents, we need to retain those OIDs
		 * somehow. This way, they are already retained for us by the walker.
		 */
        node = git_oidmap_get(walk->commits, &oid);
		GIT_ASSERT(node != NULL && node->parsed);

        if (flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON) {
			git_oidmap_set(common_oids, &node->oid, (void*) (flags & ~COMMIT_FLAG_COUNTED_AS_UNCOMMON));
            GIT_ASSERT(non_common_queued_commits > 0);
            non_common_queued_commits--;
        }

		for (p = 0; p < node->out_degree; p++) {
			git_commit_list_node *parent = node->parents[p];
            negotiation_commit_flags parent_flags =
				(negotiation_commit_flags)git_oidmap_get(common_oids, &parent->oid);

			if (flags & COMMIT_FLAG_COMMON_REF) {
				/*
				 * If a commit is a common reference, we need to process it.
				 * For any of those, we mark the parents as commons, unless they are already
				 * common references. The reason is, we skip all commits marked as common,
				 * but we don't want to skip common references, so a common reference must
				 * not be marked as common here.
				 */
                if (!(parent_flags & COMMIT_FLAG_COMMON)) {
                    if (parent_flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON)
						non_common_queued_commits--;

					parent_flags = (parent_flags & ~COMMIT_FLAG_COUNTED_AS_UNCOMMON)
						| COMMIT_FLAG_COMMON;

                    git_oidmap_set(common_oids, &parent->oid, (void *)parent_flags);
					mark_as_common(&parent->oid, 1, common_oids, walk, &non_common_queued_commits);
				}
			} else if (!(parent_flags & COMMIT_FLAG_COUNTED_AS_UNCOMMON)) {
				parent_flags |= COMMIT_FLAG_COUNTED_AS_UNCOMMON;
				git_oidmap_set(common_oids, &parent->oid, (void *)parent_flags);
				non_common_queued_commits++;
			}
		}

		git_pkt_buffer_have(&oid, &data);
		i++;

		if (i >= flush_limit) {
			flush_limit = next_flush(t, i);

            if (t->cancelled.val) {
				git_error_set(GIT_ERROR_NET, "The fetch was cancelled by the user");
				error = GIT_EUSER;
				goto on_error;
			}

			git_pkt_buffer_flush(&data);
			if (git_str_oom(&data)) {
				error = -1;
				goto on_error;
			}

			if ((error = git_smart__negotiation_step(&t->parent, data.ptr, data.size)) < 0)
				goto on_error;

			git_str_clear(&data);
			if (t->caps.multi_ack || t->caps.multi_ack_detailed) {
                ack_pkts_processing_result processing_result = ACK_PKTS_PROCESSING_RESULT_INIT;

				if (t->rpc)
					if ((error = git_pkt_buffer_wants(wants, &t->caps, &data)) < 0)
						goto on_error;

				if ((error = process_packets(&processing_result, t, common_oids,
					&data, walk, &non_common_queued_commits)) < 0)
					goto on_error;

				/* If we got a "ready" ack, we are done. */
				if (processing_result.received_ready)
					break;

                if (processing_result.received_specific_ack)
                    received_specific_ack = 1;

				/*
				 * If we iterated too many commits and didn't get a common yet,
				 * give up, unless we never received any specific ACK on previous
				 * steps.
				 */
				if (received_specific_ack && !processing_result.received_specific_ack
                    && tries > MAX_TRIES_WITHOUT_HAVE_STATEMENT)
					break;

				if (!t->rpc ||
					processing_result.have_statement_written ||
					processing_result.received_other_than_ack_common)
					tries = 0;
			} else {
				if ((error = recv_pkt(NULL, &pkt_type, t)) < 0)
					goto on_error;

				if (pkt_type == GIT_PKT_ACK) {
					break;
				} else if (pkt_type == GIT_PKT_NAK) {
					continue;
				} else {
					git_error_set(GIT_ERROR_NET, "unexpected pkt type");
					error = -1;
					goto on_error;
				}
			}
		}
	}

	if ((error = git_pkt_buffer_done(&data)) < 0)
		goto on_error;

	if (t->cancelled.val) {
		git_error_set(GIT_ERROR_NET, "the fetch was cancelled");
		error = GIT_EUSER;
		goto on_error;
	}

	if ((error = git_smart__negotiation_step(&t->parent, data.ptr, data.size)) < 0)
		goto on_error;

	git_oidmap_free(common_oids);
	git_str_dispose(&data);
	git_revwalk_free(walk);

	/* Now let's eat up whatever the server gives us */
	if (!t->caps.multi_ack && !t->caps.multi_ack_detailed) {
		if ((error = recv_pkt(NULL, &pkt_type, t)) < 0)
			return error;

		if (pkt_type != GIT_PKT_ACK && pkt_type != GIT_PKT_NAK) {
			git_error_set(GIT_ERROR_NET, "unexpected pkt type");
			return -1;
		}
	} else {
		error = wait_while_ack(t);
	}

	return error;

on_error:
	git_oidmap_free(common_oids);
	git_revwalk_free(walk);
	git_str_dispose(&data);
	return error;
}

int git_smart__shallow_roots(git_oidarray *out, git_transport *transport)
{
	transport_smart *t = (transport_smart *)transport;
	size_t len;

	GIT_ERROR_CHECK_ALLOC_MULTIPLY(&len, t->shallow_roots.size, sizeof(git_oid));

	out->count = t->shallow_roots.size;

	if (len) {
		out->ids = git__malloc(len);
		memcpy(out->ids, t->shallow_roots.ptr, len);
	} else {
		out->ids = NULL;
	}

	return 0;
}

static int no_sideband(
	transport_smart *t,
	struct git_odb_writepack *writepack,
	git_indexer_progress *stats)
{
	int recvd;

	do {
		if (t->cancelled.val) {
			git_error_set(GIT_ERROR_NET, "the fetch was cancelled by the user");
			return GIT_EUSER;
		}

		if (writepack->append(writepack, t->buffer.data, t->buffer.len, stats) < 0)
			return -1;

		git_staticstr_clear(&t->buffer);

		if ((recvd = git_smart__recv(t)) < 0)
			return recvd;
	} while(recvd > 0);

	if (writepack->commit(writepack, stats) < 0)
		return -1;

	return 0;
}

struct network_packetsize_payload
{
	git_indexer_progress_cb callback;
	void *payload;
	git_indexer_progress *stats;
	size_t last_fired_bytes;
};

static int network_packetsize(size_t received, void *payload)
{
	struct network_packetsize_payload *npp = (struct network_packetsize_payload*)payload;

	/* Accumulate bytes */
	npp->stats->received_bytes += received;

	/* Fire notification if the threshold is reached */
	if ((npp->stats->received_bytes - npp->last_fired_bytes) > NETWORK_XFER_THRESHOLD) {
		npp->last_fired_bytes = npp->stats->received_bytes;

		if (npp->callback(npp->stats, npp->payload))
			return GIT_EUSER;
	}

	return 0;
}

int git_smart__download_pack(
	git_transport *transport,
	git_repository *repo,
	git_indexer_progress *stats)
{
	transport_smart *t = (transport_smart *)transport;
	git_odb *odb;
	struct git_odb_writepack *writepack = NULL;
	int error = 0;
	struct network_packetsize_payload npp = {0};

	git_indexer_progress_cb progress_cb = t->connect_opts.callbacks.transfer_progress;
	void *progress_payload = t->connect_opts.callbacks.payload;

	memset(stats, 0, sizeof(git_indexer_progress));

	if (progress_cb) {
		npp.callback = progress_cb;
		npp.payload = progress_payload;
		npp.stats = stats;
		t->packetsize_cb = &network_packetsize;
		t->packetsize_payload = &npp;

		/* We might have something in the buffer already from negotiate_fetch */
		if (t->buffer.len > 0 && !t->cancelled.val) {
			if (t->packetsize_cb(t->buffer.len, t->packetsize_payload))
				git_atomic32_set(&t->cancelled, 1);
		}
	}

	if ((error = git_repository_odb__weakptr(&odb, repo)) < 0 ||
		((error = git_odb_write_pack(&writepack, odb, progress_cb, progress_payload)) != 0))
		goto done;

	/*
	 * If the remote doesn't support the side-band, we can feed
	 * the data directly to the pack writer. Otherwise, we need to
	 * check which one belongs there.
	 */
	if (!t->caps.side_band && !t->caps.side_band_64k) {
		error = no_sideband(t, writepack, stats);
		goto done;
	}

	do {
		git_pkt *pkt = NULL;

		/* Check cancellation before network call */
		if (t->cancelled.val) {
			git_error_clear();
			error = GIT_EUSER;
			goto done;
		}

		if ((error = recv_pkt(&pkt, NULL, t)) >= 0) {
			/* Check cancellation after network call */
			if (t->cancelled.val) {
				git_error_clear();
				error = GIT_EUSER;
			} else if (pkt->type == GIT_PKT_PROGRESS) {
				if (t->connect_opts.callbacks.sideband_progress) {
					git_pkt_progress *p = (git_pkt_progress *) pkt;

					if (p->len > INT_MAX) {
						git_error_set(GIT_ERROR_NET, "oversized progress message");
						error = GIT_ERROR;
						goto done;
					}

					error = t->connect_opts.callbacks.sideband_progress(p->data, (int)p->len, t->connect_opts.callbacks.payload);
				}
			} else if (pkt->type == GIT_PKT_DATA) {
				git_pkt_data *p = (git_pkt_data *) pkt;

				if (p->len)
					error = writepack->append(writepack, p->data, p->len, stats);
			} else if (pkt->type == GIT_PKT_FLUSH) {
				/* A flush indicates the end of the packfile */
				git__free(pkt);
				break;
			}
		}

		git_pkt_free(pkt);

		if (error < 0)
			goto done;

	} while (1);

	/*
	 * Trailing execution of progress_cb, if necessary...
	 * Only the callback through the npp datastructure currently
	 * updates the last_fired_bytes value. It is possible that
	 * progress has already been reported with the correct
	 * "received_bytes" value, but until (if?) this is unified
	 * then we will report progress again to be sure that the
	 * correct last received_bytes value is reported.
	 */
	if (npp.callback && npp.stats->received_bytes > npp.last_fired_bytes) {
		error = npp.callback(npp.stats, npp.payload);
		if (error != 0)
			goto done;
	}

	error = writepack->commit(writepack, stats);

done:
	if (writepack)
		writepack->free(writepack);
	if (progress_cb) {
		t->packetsize_cb = NULL;
		t->packetsize_payload = NULL;
	}

	return error;
}

static int gen_pktline(git_str *buf, git_push *push)
{
	push_spec *spec;
	char *option;
	size_t i, len;
	char old_id[GIT_OID_MAX_HEXSIZE + 1], new_id[GIT_OID_MAX_HEXSIZE + 1];
	size_t old_id_len, new_id_len;

	git_vector_foreach(&push->specs, i, spec) {
		len = strlen(spec->refspec.dst) + 7;

		if (i == 0) {
			/* Need a leading \0 */
			++len;

			if (push->report_status)
				len += strlen(GIT_CAP_REPORT_STATUS) + 1;

			if (git_vector_length(&push->remote_push_options) > 0)
				len += strlen(GIT_CAP_PUSH_OPTIONS) + 1;

			len += strlen(GIT_CAP_SIDE_BAND_64K) + 1;
		}

		old_id_len = git_oid_hexsize(git_oid_type(&spec->roid));
		new_id_len = git_oid_hexsize(git_oid_type(&spec->loid));

		len += (old_id_len + new_id_len);

		git_oid_fmt(old_id, &spec->roid);
		old_id[old_id_len] = '\0';

		git_oid_fmt(new_id, &spec->loid);
		new_id[new_id_len] = '\0';

		git_str_printf(buf, "%04"PRIxZ"%.*s %.*s %s", len,
			(int)old_id_len, old_id, (int)new_id_len, new_id,
			spec->refspec.dst);

		if (i == 0) {
			git_str_putc(buf, '\0');

			/* Core git always starts their capabilities string with a space */
			if (push->report_status) {
				git_str_putc(buf, ' ');
				git_str_printf(buf, GIT_CAP_REPORT_STATUS);
			}
			if (git_vector_length(&push->remote_push_options) > 0) {
				git_str_putc(buf, ' ');
				git_str_printf(buf, GIT_CAP_PUSH_OPTIONS);
			}
			git_str_putc(buf, ' ');
			git_str_printf(buf, GIT_CAP_SIDE_BAND_64K);
		}

		git_str_putc(buf, '\n');
	}

	if (git_vector_length(&push->remote_push_options) > 0) {
		git_str_printf(buf, "0000");
		git_vector_foreach(&push->remote_push_options, i, option) {
			git_str_printf(buf, "%04"PRIxZ"%s", strlen(option) + 4 , option);
		}
	}

	git_str_puts(buf, "0000");
	return git_str_oom(buf) ? -1 : 0;
}

static int add_push_report_pkt(git_push *push, git_pkt *pkt)
{
	push_status *status;

	switch (pkt->type) {
		case GIT_PKT_OK:
			status = git__calloc(1, sizeof(push_status));
			GIT_ERROR_CHECK_ALLOC(status);
			status->msg = NULL;
			status->ref = git__strdup(((git_pkt_ok *)pkt)->ref);
			if (!status->ref ||
				git_vector_insert(&push->status, status) < 0) {
				git_push_status_free(status);
				return -1;
			}
			break;
		case GIT_PKT_NG:
			status = git__calloc(1, sizeof(push_status));
			GIT_ERROR_CHECK_ALLOC(status);
			status->ref = git__strdup(((git_pkt_ng *)pkt)->ref);
			status->msg = git__strdup(((git_pkt_ng *)pkt)->msg);
			if (!status->ref || !status->msg ||
				git_vector_insert(&push->status, status) < 0) {
				git_push_status_free(status);
				return -1;
			}
			break;
		case GIT_PKT_UNPACK:
			push->unpack_ok = ((git_pkt_unpack *)pkt)->unpack_ok;
			break;
		case GIT_PKT_FLUSH:
			return GIT_ITEROVER;
		default:
			git_error_set(GIT_ERROR_NET, "report-status: protocol error");
			return -1;
	}

	return 0;
}

static int add_push_report_sideband_pkt(git_push *push, git_pkt_data *data_pkt, git_str *data_pkt_buf)
{
	git_pkt *pkt;
	git_pkt_parse_data pkt_parse_data = { 0 };
	const char *line, *line_end = NULL;
	size_t line_len;
	int error;
	int reading_from_buf = data_pkt_buf->size > 0;

	if (reading_from_buf) {
		/* We had an existing partial packet, so add the new
		 * packet to the buffer and parse the whole thing */
		git_str_put(data_pkt_buf, data_pkt->data, data_pkt->len);
		line = data_pkt_buf->ptr;
		line_len = data_pkt_buf->size;
	}
	else {
		line = data_pkt->data;
		line_len = data_pkt->len;
	}

	while (line_len > 0) {
		error = git_pkt_parse_line(&pkt, &line_end, line, line_len, &pkt_parse_data);

		if (error == GIT_EBUFS) {
			/* Buffer the data when the inner packet is split
			 * across multiple sideband packets */
			if (!reading_from_buf)
				git_str_put(data_pkt_buf, line, line_len);
			error = 0;
			goto done;
		}
		else if (error < 0)
			goto done;

		/* Advance in the buffer */
		line_len -= (line_end - line);
		line = line_end;

		error = add_push_report_pkt(push, pkt);

		git_pkt_free(pkt);

		if (error < 0 && error != GIT_ITEROVER)
			goto done;
	}

	error = 0;

done:
	if (reading_from_buf)
		git_str_consume(data_pkt_buf, line_end);
	return error;
}

static int parse_report(transport_smart *transport, git_push *push)
{
	git_pkt *pkt = NULL;
	git_pkt_parse_data pkt_parse_data = { 0 };
	const char *line_end = NULL;
	int error, recvd;
	git_str data_pkt_buf = GIT_STR_INIT;

	for (;;) {
		if (transport->buffer.len > 0)
			error = git_pkt_parse_line(&pkt, &line_end,
				   transport->buffer.data,
				   transport->buffer.len,
				   &pkt_parse_data);
		else
			error = GIT_EBUFS;

		if (error < 0 && error != GIT_EBUFS) {
			error = -1;
			goto done;
		}

		if (error == GIT_EBUFS) {
			if ((recvd = git_smart__recv(transport)) < 0) {
				error = recvd;
				goto done;
			}

			if (recvd == 0) {
				git_error_set(GIT_ERROR_NET, "could not read report from remote repository");
				error = GIT_EEOF;
				goto done;
			}
			continue;
		}

		git_staticstr_consume(&transport->buffer, line_end);
		error = 0;

		switch (pkt->type) {
			case GIT_PKT_DATA:
				/* This is a sideband packet which contains other packets */
				error = add_push_report_sideband_pkt(push, (git_pkt_data *)pkt, &data_pkt_buf);
				break;
			case GIT_PKT_ERR:
				git_error_set(GIT_ERROR_NET, "report-status: Error reported: %s",
					((git_pkt_err *)pkt)->error);
				error = -1;
				break;
			case GIT_PKT_PROGRESS:
				if (transport->connect_opts.callbacks.sideband_progress) {
					git_pkt_progress *p = (git_pkt_progress *) pkt;

					if (p->len > INT_MAX) {
						git_error_set(GIT_ERROR_NET, "oversized progress message");
						error = GIT_ERROR;
						goto done;
					}

					error = transport->connect_opts.callbacks.sideband_progress(p->data, (int)p->len, transport->connect_opts.callbacks.payload);
				}
				break;
			default:
				error = add_push_report_pkt(push, pkt);
				break;
		}

		git_pkt_free(pkt);

		/* add_push_report_pkt returns GIT_ITEROVER when it receives a flush */
		if (error == GIT_ITEROVER) {
			error = 0;
			if (data_pkt_buf.size > 0) {
				/* If there was data remaining in the pack data buffer,
				 * then the server sent a partial pkt-line */
				git_error_set(GIT_ERROR_NET, "incomplete pack data pkt-line");
				error = GIT_ERROR;
			}
			goto done;
		}

		if (error < 0) {
			goto done;
		}
	}
done:
	git_str_dispose(&data_pkt_buf);
	return error;
}

static int add_ref_from_push_spec(git_vector *refs, push_spec *push_spec)
{
	git_pkt_ref *added = git__calloc(1, sizeof(git_pkt_ref));
	GIT_ERROR_CHECK_ALLOC(added);

	added->type = GIT_PKT_REF;
	git_oid_cpy(&added->head.oid, &push_spec->loid);
	added->head.name = git__strdup(push_spec->refspec.dst);

	if (!added->head.name ||
		git_vector_insert(refs, added) < 0) {
		git_pkt_free((git_pkt *)added);
		return -1;
	}

	return 0;
}

static int update_refs_from_report(
	git_vector *refs,
	git_vector *push_specs,
	git_vector *push_report)
{
	git_pkt_ref *ref;
	push_spec *push_spec;
	push_status *push_status;
	size_t i, j, refs_len;
	int cmp;

	/* For each push spec we sent to the server, we should have
	 * gotten back a status packet in the push report */
	if (push_specs->length != push_report->length) {
		git_error_set(GIT_ERROR_NET, "report-status: protocol error");
		return -1;
	}

	/* We require that push_specs be sorted with push_spec_rref_cmp,
	 * and that push_report be sorted with push_status_ref_cmp */
	git_vector_sort(push_specs);
	git_vector_sort(push_report);

	git_vector_foreach(push_specs, i, push_spec) {
		push_status = git_vector_get(push_report, i);

		/* For each push spec we sent to the server, we should have
		 * gotten back a status packet in the push report which matches */
		if (strcmp(push_spec->refspec.dst, push_status->ref)) {
			git_error_set(GIT_ERROR_NET, "report-status: protocol error");
			return -1;
		}
	}

	/* We require that refs be sorted with ref_name_cmp */
	git_vector_sort(refs);
	i = j = 0;
	refs_len = refs->length;

	/* Merge join push_specs with refs */
	while (i < push_specs->length && j < refs_len) {
		push_spec = git_vector_get(push_specs, i);
		push_status = git_vector_get(push_report, i);
		ref = git_vector_get(refs, j);

		cmp = strcmp(push_spec->refspec.dst, ref->head.name);

		/* Iterate appropriately */
		if (cmp <= 0) i++;
		if (cmp >= 0) j++;

		/* Add case */
		if (cmp < 0 &&
			!push_status->msg &&
			add_ref_from_push_spec(refs, push_spec) < 0)
			return -1;

		/* Update case, delete case */
		if (cmp == 0 &&
			!push_status->msg)
			git_oid_cpy(&ref->head.oid, &push_spec->loid);
	}

	for (; i < push_specs->length; i++) {
		push_spec = git_vector_get(push_specs, i);
		push_status = git_vector_get(push_report, i);

		/* Add case */
		if (!push_status->msg &&
			add_ref_from_push_spec(refs, push_spec) < 0)
			return -1;
	}

	/* Remove any refs which we updated to have a zero OID. */
	git_vector_rforeach(refs, i, ref) {
		if (git_oid_is_zero(&ref->head.oid)) {
			git_vector_remove(refs, i);
			git_pkt_free((git_pkt *)ref);
		}
	}

	git_vector_sort(refs);

	return 0;
}

struct push_packbuilder_payload
{
	git_smart_subtransport_stream *stream;
	git_packbuilder *pb;
	git_push_transfer_progress_cb cb;
	void *cb_payload;
	size_t last_bytes;
	uint64_t last_progress_report_time;
};

static int stream_thunk(void *buf, size_t size, void *data)
{
	int error = 0;
	struct push_packbuilder_payload *payload = data;

	if ((error = payload->stream->write(payload->stream, (const char *)buf, size)) < 0)
		return error;

	if (payload->cb) {
		uint64_t current_time = git_time_monotonic();
		uint64_t elapsed = current_time - payload->last_progress_report_time;
		payload->last_bytes += size;

		if (elapsed >= MIN_PROGRESS_UPDATE_INTERVAL) {
			payload->last_progress_report_time = current_time;
			error = payload->cb(payload->pb->nr_written, payload->pb->nr_objects, payload->last_bytes, payload->cb_payload);
		}
	}

	return error;
}

int git_smart__push(git_transport *transport, git_push *push)
{
	transport_smart *t = (transport_smart *)transport;
	git_remote_callbacks *cbs = &t->connect_opts.callbacks;
	struct push_packbuilder_payload packbuilder_payload = {0};
	git_str pktline = GIT_STR_INIT;
	int error = 0, need_pack = 0;
	push_spec *spec;
	unsigned int i;

	packbuilder_payload.pb = push->pb;

	if (cbs && cbs->push_transfer_progress) {
		packbuilder_payload.cb = cbs->push_transfer_progress;
		packbuilder_payload.cb_payload = cbs->payload;
	}

#ifdef PUSH_DEBUG
{
	git_remote_head *head;
	char hex[GIT_OID_MAX_HEXSIZE+1], hex[GIT_OID_MAX_HEXSIZE] = '\0';

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

	/*
	 * Figure out if we need to send a packfile; which is in all
	 * cases except when we only send delete commands
	 */
	git_vector_foreach(&push->specs, i, spec) {
		if (spec->refspec.src && spec->refspec.src[0] != '\0') {
			need_pack = 1;
			break;
		}
	}

	/* prepare pack before sending pack header to avoid timeouts */
	if (need_pack && ((error = git_packbuilder__prepare(push->pb))) < 0)
		goto done;

	if ((error = git_smart__get_push_stream(t, &packbuilder_payload.stream)) < 0 ||
		(error = gen_pktline(&pktline, push)) < 0 ||
		(error = packbuilder_payload.stream->write(packbuilder_payload.stream, git_str_cstr(&pktline), git_str_len(&pktline))) < 0)
		goto done;

	if (need_pack &&
		(error = git_packbuilder_foreach(push->pb, &stream_thunk, &packbuilder_payload)) < 0)
		goto done;

	/* If we sent nothing or the server doesn't support report-status, then
	 * we consider the pack to have been unpacked successfully */
	if (!push->specs.length || !push->report_status)
		push->unpack_ok = 1;
	else if ((error = parse_report(t, push)) < 0)
		goto done;

	/* If progress is being reported write the final report */
	if (cbs && cbs->push_transfer_progress) {
		error = cbs->push_transfer_progress(
					push->pb->nr_written,
					push->pb->nr_objects,
					packbuilder_payload.last_bytes,
					cbs->payload);

		if (error < 0)
			goto done;
	}

	if (push->status.length) {
		error = update_refs_from_report(&t->refs, &push->specs, &push->status);
		if (error < 0)
			goto done;

		error = git_smart__update_heads(t, NULL);
	}

done:
	git_str_dispose(&pktline);
	return error;
}
