#ifndef INCLUDE_cl_push_util_h__
#define INCLUDE_cl_push_util_h__

#include "git2/oid.h"

/* Constant for zero oid */
extern const git_oid OID_ZERO;

/**
 * Macro for initializing git_remote_callbacks to use test helpers that
 * record data in a record_callbacks_data instance.
 * @param data pointer to a record_callbacks_data instance
 */
#define RECORD_CALLBACKS_INIT(data) \
	{ GIT_REMOTE_CALLBACKS_VERSION, NULL, NULL, record_update_tips_cb, data }

typedef struct {
	char *name;
	git_oid *old_oid;
	git_oid *new_oid;
} updated_tip;

typedef struct {
	git_vector updated_tips;
} record_callbacks_data;

typedef struct {
	const char *name;
	const git_oid *oid;
} expected_ref;

void updated_tip_free(updated_tip *t);

void record_callbacks_data_clear(record_callbacks_data *data);

/**
 * Callback for git_remote_update_tips that records updates
 *
 * @param data (git_vector *) of updated_tip instances
 */
int record_update_tips_cb(const char *refname, const git_oid *a, const git_oid *b, void *data);

/**
 * Callback for git_remote_list that adds refspecs to delete each ref
 *
 * @param head a ref on the remote
 * @param payload a git_push instance
 */
int delete_ref_cb(git_remote_head *head, void *payload);

/**
 * Callback for git_remote_list that adds refspecs to vector
 *
 * @param head a ref on the remote
 * @param payload (git_vector *) of git_remote_head instances
 */
int record_ref_cb(git_remote_head *head, void *payload);

/**
 * Verifies that refs on remote stored by record_ref_cb match the expected
 * names, oids, and order.
 *
 * @param actual_refs actual refs stored by record_ref_cb()
 * @param expected_refs expected remote refs
 * @param expected_refs_len length of expected_refs
 */
void verify_remote_refs(git_vector *actual_refs, const expected_ref expected_refs[], size_t expected_refs_len);

#endif /* INCLUDE_cl_push_util_h__ */
