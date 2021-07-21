/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

#include "git2/sys/refdb_backend.h"
#include "git2/sys/refs.h"

#include "refdb.h"
#include "reflog.h"
#include "reftable.h"
#include "signature.h"
#include "wildmatch.h"

typedef struct {
	git_refdb_backend parent;
	git_repository *repo;
	struct reftable_stack *stack;
} refdb_reftable;

typedef struct {
	git_reference_iterator parent;
	struct reftable_merged_table *table;
	struct reftable_iterator iter;
	struct reftable_ref_record ref;
	const char *glob;
} refdb_reftable_iterator;

typedef struct {
	struct reftable_ref_record *refs;
	struct reftable_log_record *logs;
	int nrefs;
	int nlogs;
	uint64_t min_index;
	uint64_t max_index;
} refdb_reftable_writer_args;

static int reftable_error(int error, const char *msg)
{
	int class;

	switch ((enum reftable_error) error) {
	case REFTABLE_NOT_EXIST_ERROR:
		class = GIT_ENOTFOUND;
		break;
	case REFTABLE_LOCK_ERROR:
		class = GIT_ELOCKED;
		break;
	case REFTABLE_NAME_CONFLICT:
		class = GIT_EDIRECTORY;
		break;
	default:
		class = GIT_ERROR;
		break;
	}

	git_error_set(GIT_ERROR_REFERENCE, "%s: %s",
		      msg, reftable_error_str(error));
	return class;
}

static int refdb_reftable_reference_from_record(git_reference **out, struct reftable_ref_record *record)
{
	git_reference *ref;
	int error = 0;

	if (record->target) {
		ref = git_reference__alloc_symbolic(record->refname, record->target);
	} else {
		git_oid oid, peeled = {{0}};

		if ((error = git_oid_fromraw(&oid, record->value)) < 0)
			goto out;
		if (record->target_value)
			if ((error = git_oid_fromraw(&peeled, record->target_value)) < 0)
				goto out;
		ref = git_reference__alloc(record->refname, &oid, &peeled);
	}
	GIT_ERROR_CHECK_ALLOC(ref);

	*out = ref;
out:
	return error;
}

static int verify_update_error(int error, const char *name, const char *msg)
{
	git_error_set(GIT_ERROR_REFERENCE, "failed updating reference '%s': %s", name, msg);
	return error;
}

static int refdb_reftable_verify_update(refdb_reftable *backend, const char *name, int force, const git_oid *expected_oid, const char *expected_target, struct reftable_ref_record *new)
{
	struct reftable_ref_record ref = {0};
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0)
		return reftable_error(error, "failed updating reftable for update");

	if ((error = reftable_stack_read_ref(backend->stack, name, &ref)) < 0)
		return reftable_error(error, "failed reading reference");

	/*
	 * If not forcing an update and we don't expect anything to exist,
	 * reading the reference should have turned up empty.
	 */
	if (!force && !error && !expected_oid && !expected_target) {
		error = verify_update_error(GIT_EEXISTS, name, "a reference with that name exists already");
		goto out;
	}

	/* Check that we found a reference in case we expect one. */
	if ((expected_oid || expected_target) && error == 1) {
		error = verify_update_error(GIT_ENOTFOUND, name, "old reference not found");
		goto out;
	}

	/*
	 * If expecting a directory reference but it is a symbolic one or vice
	 * versa, we need to return an error.
	 */
	if ((expected_oid && ref.target) || (expected_target && ref.value)) {
		error = verify_update_error(GIT_EMODIFIED, name,
					    "old reference type does not match");
		goto out;
	}

	/* Check that the expected OID matches. */
	if (expected_oid && ref.value) {
		git_oid oid;
		if ((error = git_oid_fromraw(&oid, ref.value)) < 0)
			goto out;
		if (git_oid_cmp(&oid, expected_oid) != 0) {
			error = verify_update_error(GIT_EMODIFIED, name,
						    "old reference value does not match");
			goto out;
		}
	}

	/* Check that the expected reference matches. */
	if (expected_target && ref.target) {
		if (git__strcmp(ref.target, expected_target) != 0) {
			error = verify_update_error(GIT_EMODIFIED, name,
						    "old reference value does not match");
			goto out;
		}
	}

	/* In case the new reference has the same value as the old one, we need to skip the update. */
	if (new) {
		if (new->target && ref.target && !strcmp(new->target, ref.target)) {
			error = GIT_PASSTHROUGH;
			goto out;
		}
		if (new->value && ref.value && !memcmp(new->value, ref.value, GIT_OID_RAWSZ)) {
			error = GIT_PASSTHROUGH;
			goto out;
		}
	}

	error = 0;
out:
	reftable_ref_record_clear(&ref);
	return error;
}

static int refdb_reftable_log_fill(struct reftable_log_record *out,
				     const git_signature *who,
				     const git_oid *old_id,
				     const git_oid *new_id,
				     const char *reference,
				     const char *message)
{
	memset(out, 0, sizeof(*out));
	if (who) {
		out->name = who->name;
		out->email = who->email;
		out->time = who->when.time;
		out->tz_offset = who->when.offset;
	}
	if (old_id)
		out->old_hash = (unsigned char *) old_id->id;
	if (new_id)
		out->new_hash = (unsigned char *) new_id->id;
	out->refname = (char *) reference;
	out->message = (char *) message;
	return 0;
}

static int refdb_reftable_writer(struct reftable_writer *wr, void *_arg)
{
	refdb_reftable_writer_args *arg = (refdb_reftable_writer_args *) _arg;

	reftable_writer_set_limits(wr, arg->min_index, arg->max_index);

	if (arg->nrefs && reftable_writer_add_refs(wr, arg->refs, arg->nrefs) < 0)
		return -1;
	if (arg->nlogs && reftable_writer_add_logs(wr, arg->logs, arg->nlogs) < 0)
		return -1;

	return 0;
}

static int refdb_reftable_write_refs(refdb_reftable *backend,
				       struct reftable_ref_record *refs, int nrefs,
				       struct reftable_log_record *logs, int nlogs)
{
	refdb_reftable_writer_args arg = {
		refs, logs, nrefs, nlogs
	};
	int i, error;

	arg.min_index = arg.max_index = reftable_stack_next_update_index(backend->stack);

	for (i = 0; i < nrefs; i++)
		refs[i].update_index = arg.min_index;
	for (i = 0; i < nlogs; i++)
		logs[i].update_index = arg.min_index;

	if ((error = reftable_stack_add(backend->stack, refdb_reftable_writer, &arg)) < 0)
		return reftable_error(error, "failed writing records");

	return 0;
}

static int refdb_reftable_write_logs(refdb_reftable *backend,
				       struct reftable_log_record *logs, int nlogs)
{
	refdb_reftable_writer_args arg = {
		NULL, logs, 0, nlogs
	};
	int i, error;

	/*
	 * Log records are keyed only by name, so they need different update
	 * indices in case there's multiple entries with the same name. To
	 * avoid any heavy-handed logic, let's just give each record a
	 * different index.
	 */
	arg.min_index = reftable_stack_next_update_index(backend->stack);
	arg.max_index = arg.min_index + nlogs;

	for (i = 0; i < nlogs; i++)
		logs[i].update_index = arg.min_index + i;

	if ((error = reftable_stack_add(backend->stack, refdb_reftable_writer, &arg)) < 0)
		return reftable_error(error, "failed writing records");

	return 0;
}

static int refdb_reftable_exists(
	int *exists,
	git_refdb_backend *_backend,
	const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_ref_record ref = { 0 };
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0 ||
	    (error = reftable_stack_read_ref(backend->stack, refname, &ref)) < 0)
		return reftable_error(error, "failed reading reference");
	*exists = (error == 0);

	reftable_ref_record_clear(&ref);
	return 0;
}

static int refdb_reftable_lookup(
	git_reference **out,
	git_refdb_backend *_backend,
	const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_ref_record ref = {0};
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0 ||
	    (error = reftable_stack_read_ref(backend->stack, refname, &ref)) < 0) {
		error = reftable_error(error, "failed reference lookup");
		goto out;
	}
	if (error > 0) {
		git_error_set(GIT_ERROR_REFERENCE, "reference '%s' not found", refname);
		error = GIT_ENOTFOUND;
		goto out;
	}

	if ((error = refdb_reftable_reference_from_record(out, &ref)) < 0)
		goto out;

out:
	reftable_ref_record_clear(&ref);
	return error;
}

static int refdb_reftable_iterator_next_record(refdb_reftable_iterator *it)
{
	while (1) {
		int error;

		if ((error = reftable_iterator_next_ref(&it->iter, &it->ref)) != 0) {
			if (error > 0)
				return GIT_ITEROVER;
			return reftable_error(error, "failed retrieving next record");
		}

		if (it->glob && wildmatch(it->glob, it->ref.refname, 0) != 0)
			continue;

		return 0;
	}
}

static int refdb_reftable_iterator_next(git_reference **out, git_reference_iterator *_it)
{
	refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
	int error;

	if ((error = refdb_reftable_iterator_next_record(it)) < 0)
		return error;

	return refdb_reftable_reference_from_record(out, &it->ref);
}

static int refdb_reftable_iterator_next_name(const char **out, git_reference_iterator *_it)
{
	refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
	int error;

	if ((error = refdb_reftable_iterator_next_record(it)) < 0)
		return error;
	*out = it->ref.refname;

	return error;
}

static void refdb_reftable_iterator_free(git_reference_iterator *_it)
{
	if (_it) {
		refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
		reftable_iterator_destroy(&it->iter);
		reftable_ref_record_clear(&it->ref);
		git__free((char *) it->glob);
		git__free(it);
	}
}

static int refdb_reftable_iterator_new(
	git_reference_iterator **out, git_refdb_backend *_backend, const char *glob)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_merged_table *table = NULL;
	refdb_reftable_iterator *it = NULL;
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0)
		goto out;

	table = reftable_stack_merged_table(backend->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	it = git__calloc(1, sizeof(*it));
	GIT_ERROR_CHECK_ALLOC(it);
	it->parent.next = refdb_reftable_iterator_next;
	it->parent.next_name = refdb_reftable_iterator_next_name;
	it->parent.free = refdb_reftable_iterator_free;
	it->table = table;

	if (glob) {
		const char *separator = NULL;
		const char *pos;

		for (pos = glob; *pos; pos++) {
			switch (*pos) {
			case '?':
			case '*':
			case '[':
			case '\\':
				break;
			case '/':
				separator = pos;
				/* FALLTHROUGH */
			default:
				continue;
			}
			break;
		}

		if (separator) {
			it->glob = git__strdup(glob);
		} else {
			git_buf pattern = GIT_BUF_INIT;
			if ((git_buf_printf(&pattern, "refs/%s", glob)) < 0)
				goto out;
			it->glob = git_buf_detach(&pattern);
		}
	} else {
		it->glob = git__strdup("refs/*");
	}
	GIT_ERROR_CHECK_ALLOC(it->glob);

	if ((error = reftable_merged_table_seek_ref(table, &it->iter, "")) < 0)
		goto out;

	*out = &it->parent;
out:
	if (error < 0) {
		reftable_merged_table_free(table);
		if (it)
			refdb_reftable_iterator_free(&it->parent);
	}
	return 0;
}

static int refdb_reftable_write(
	git_refdb_backend *_backend,
	const git_reference *ref,
	int force,
	const git_signature *who,
	const char *message,
	const git_oid *expected_oid,
	const char *expected_target)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_log_record log_records[2] = {{0}};
	struct reftable_ref_record ref_record = {0};
	int error, write_reflog, write_head_reflog = 0, nlogs = 0;
	git_oid old_id, new_id;
	git_object *peeled = NULL;
	const char *refname;
	git_refdb *refdb;

	if ((refname = git_reference_name(ref)) == NULL) {
		git_error_set(GIT_ERROR_REFERENCE, "can not write reference with no name");
		return -1;
	}
	ref_record.refname = (char *) refname;

	if (git_reference_name_to_id(&old_id, backend->repo, refname) < 0)
		memset(&old_id, 0, sizeof(old_id));

	switch (git_reference_type(ref)) {
		case GIT_REFERENCE_SYMBOLIC:
			ref_record.target = (char *) git_reference_symbolic_target(ref);
			break;
		case GIT_REFERENCE_DIRECT:
			git_oid_cpy(&new_id, git_reference_target(ref));
			ref_record.value = (unsigned char *) new_id.id;

			if ((error = git_reference_peel(&peeled, ref, GIT_OBJECT_COMMIT)) == 0)
				ref_record.target_value = (unsigned char *) git_object_id(peeled)->id;

			break;
		default:
			return -1;
	}

	if ((error = git_repository_refdb__weakptr(&refdb, backend->repo)) < 0 ||
	    (error = git_refdb_should_write_reflog(&write_reflog, refdb, ref)) < 0)
		goto out;

	if (write_reflog) {
		/* If we're detachching HEAD, then no reflog should be written */
		if (ref->type == GIT_REFERENCE_SYMBOLIC &&
		    git_reference_name_to_id(&new_id, backend->repo, git_reference_symbolic_target(ref)) < 0)
			write_reflog = 0;
		else if ((error = git_refdb_should_write_head_reflog(&write_head_reflog, refdb, ref)) < 0)
			goto out;
		else
			nlogs = write_head_reflog ? 2 : 1;
	}

	if (write_reflog) {
		if ((error = refdb_reftable_log_fill(&log_records[0], who, &old_id, &new_id, refname, message)) < 0)
			goto out;
	}

	if (write_head_reflog) {
		if ((error = refdb_reftable_log_fill(&log_records[1], who, &old_id, &new_id, GIT_HEAD_FILE, message)) < 0)
			goto out;
	}

	if ((error = refdb_reftable_verify_update(backend, refname, force, expected_oid, expected_target, &ref_record)) < 0) {
		if (error == GIT_PASSTHROUGH)
			error = 0;
		goto out;
	}

	if ((error = refdb_reftable_write_refs(backend, &ref_record, 1, log_records, nlogs)) < 0)
		goto out;

out:
	git_object_free(peeled);
	return error;
}

static int refdb_reftable_delete(
	git_refdb_backend *_backend,
	const char *refname,
	const git_oid *old_id,
	const char *old_target)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_ref_record ref = {0};
	struct reftable_log_record log = {0};
	int error, force = 0;

	/* If given no old values, we just blindly delete */
	if (!old_id && !old_target)
		force = 1;

	if ((error = refdb_reftable_verify_update(backend, refname, force, old_id, old_target, NULL)) < 0)
		goto out;

	ref.refname = (char *)refname;

	if ((error = refdb_reftable_log_fill(&log, NULL, old_id, NULL, refname, NULL)) < 0 ||
	    (error = refdb_reftable_write_refs(backend, &ref, 1, &log, 1)) < 0)
		goto out;

out:
	return error;
}

static int refdb_reftable_rename(
	git_reference **out,
	git_refdb_backend *_backend,
	const char *old_name,
	const char *new_name,
	int force,
	const git_signature *who,
	const char *message)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_ref_record refs[2] = {{0}, {0}}, existing = {0}, renamed = {0};
	struct reftable_log_record logs[2] = {{0}, {0}};
	git_oid oid;
	int error;

	if ((error = reftable_stack_read_ref(backend->stack, old_name, &existing)) != 0) {
		if (error > 0)
			error = GIT_ENOTFOUND;
		goto out;
	}

	if (existing.value && (error = git_oid_fromraw(&oid, existing.value)) < 0)
		goto out;

	refs[0].refname = (char *) old_name;
	refs[1].refname = (char *) new_name;
	refs[1].value = existing.value;
	refs[1].target_value = existing.target_value;
	refs[1].target = existing.target;

	if ((error = refdb_reftable_verify_update(backend, new_name, force, NULL, NULL, &refs[1])) < 0) {
		if (error == GIT_PASSTHROUGH)
			error = 0;
		goto out;
	}

	/* Copy new record as the reftable library will sort it away under our feet. */
	renamed = refs[1];

	if ((error = refdb_reftable_log_fill(&logs[0], who, &oid, NULL, old_name, message)) < 0 ||
	    (error = refdb_reftable_log_fill(&logs[1], who, &oid, &oid, new_name, message)) < 0 ||
	    (error = refdb_reftable_write_refs(backend, refs, 2, logs, 2)) < 0 ||
	    (error = refdb_reftable_reference_from_record(out, &renamed)) < 0)
		goto out;

out:
	reftable_ref_record_clear(&existing);
	return error;
}

static void refdb_reftable_free(git_refdb_backend *_backend)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	reftable_stack_destroy(backend->stack);
	git__free(backend);
}

static int refdb_reftable_has_log(git_refdb_backend *_backend, const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_log_record record = {0};
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0 ||
	    (error = reftable_stack_read_log(backend->stack, refname, &record)) < 0)
		return error;
	if (error > 0)
		return 0;

	reftable_log_record_clear(&record);
	return 1;
}

static int refdb_reftable_ensure_log(git_refdb_backend *_backend, const char *name)
{
	/* TODO: doesn't make any sense in the context of reftable? */
	GIT_UNUSED(_backend);
	GIT_UNUSED(name);
	return 0;
}

static int refdb_reftable_reflog_read(git_reflog **out, git_refdb_backend *_backend, const char *name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_merged_table *table = NULL;
	struct reftable_log_record record = {0};
	struct reftable_iterator iter = {0};
	git_reflog *reflog = NULL;
	int error;

	if ((error = reftable_stack_reload(backend->stack)) < 0)
		goto out;

	reflog = git__calloc(1, sizeof(git_reflog));
	GIT_ERROR_CHECK_ALLOC(reflog);

	reflog->ref_name = git__strdup(name);
	GIT_ERROR_CHECK_ALLOC(reflog->ref_name);

	if (git_vector_init(&reflog->entries, 0, NULL) < 0)
		goto out;

	table = reftable_stack_merged_table(backend->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	if ((error = reftable_merged_table_seek_log(table, &iter, name)) < 0) {
		error = reftable_error(error, "could not get reflog entries");
		goto out;
	}

	while (1) {
		git_signature *signature;
		git_reflog_entry *entry;

		if ((error = reftable_iterator_next_log(&iter, &record)) < 0) {
			error = reftable_error(error, "could not get next reflog entry");
			goto out;
		}
		if (error > 0 || strcmp(record.refname, name))
			break;

		if ((error = git_signature_new(&signature,
					       record.name, record.email,
					       record.time, record.tz_offset)) < 0)
			continue;

		entry = git__calloc(1, sizeof(*entry));
		GIT_ERROR_CHECK_ALLOC(entry);
		entry->committer = signature;

		/*
		 * Compatibility hacks with the file-based reflog implementation. */
		if (record.message && record.message[0] == '\0') {
			git__free(record.message);
		} else if (record.message) {
			size_t len = strlen(record.message);
			while (len) {
				if (!git__isspace(record.message[len - 1]))
					break;
				len--;
			}
			record.message[len] = '\0';
			entry->msg = record.message;

		}
		record.message = NULL;

		if ((error = git_oid_fromraw(&entry->oid_old, record.old_hash)) < 0 ||
		    (error = git_oid_fromraw(&entry->oid_cur, record.new_hash)) < 0)
			goto out;

		if ((git_vector_insert(&reflog->entries, entry)) < 0)
			goto out;
	}
	error = 0;

	/* Logs are expected in recency-order. */
	git_vector_reverse(&reflog->entries);

	*out = reflog;
	reflog = NULL;
out:
	git_reflog_free(reflog);
	reftable_log_record_clear(&record);
	reftable_iterator_destroy(&iter);
	return error;
}

static int refdb_reftable_reflog_entry_equal(const git_reflog_entry *entry, const struct reftable_log_record *record)
{
	if (record->time != (uint64_t) entry->committer->when.time ||
	    record->tz_offset != entry->committer->when.offset)
		return 0;
	if (!record->old_hash && !git_oid_is_zero(&entry->oid_old))
		return 0;
	if (!record->new_hash && !git_oid_is_zero(&entry->oid_cur))
		return 0;
	if (record->old_hash && memcmp(record->old_hash, entry->oid_old.id, GIT_OID_RAWSZ))
		return 0;
	if (record->new_hash && memcmp(record->new_hash, entry->oid_cur.id, GIT_OID_RAWSZ))
		return 0;
	if (record->message && entry->msg && strcmp(record->message, entry->msg))
		return 0;
	return 1;
}

static int refdb_reftable_reflog_write(git_refdb_backend *_backend, git_reflog *reflog)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_log_record persisted = {0}, *logs = NULL;
	struct reftable_merged_table *table = NULL;
	struct reftable_iterator iter = {0};
	size_t nentries, i;
	int error, j;

	if ((error = reftable_stack_reload(backend->stack)) < 0)
		goto out;

	table = reftable_stack_merged_table(backend->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	if ((error = reftable_merged_table_seek_log(table, &iter, reflog->ref_name)) < 0) {
		error = reftable_error(error, "could not get reflog entries");
		goto out;
	}

	nentries = git_reflog_entrycount(reflog);
	logs = git__calloc(nentries, sizeof(*logs));
	GIT_ERROR_CHECK_ALLOC(logs);

	/*
	 * Our `git_reflog` structure doesn't provide any information about which entries have been
	 * deleted or added, is the refdb_fs backend simply re-wrote the complete file every time.
	 * We thus need to determine manually which entries we'll have to add or delete.
	 */
	for (i = 0, j = 0; i < nentries; i++) {
		const git_reflog_entry *entry;

		if ((entry = git_reflog_entry_byindex(reflog, nentries - i - 1)) == NULL) {
			error = -1;
			goto out;
		}

		if ((error = reftable_iterator_next_log(&iter, &persisted)) < 0) {
			error = reftable_error(error, "could not get next reflog entry");
			goto out;
		}

		/* If we already have a persisted reflog entry,then we just skip writing it. */
		if (error == 0 && refdb_reftable_reflog_entry_equal(entry, &persisted))
			continue;

		if ((error = refdb_reftable_log_fill(&logs[j++], entry->committer, &entry->oid_old,
						     &entry->oid_cur, reflog->ref_name, entry->msg)) < 0)
			goto out;
	}

	if ((error = refdb_reftable_write_logs(backend, logs, j)) < 0)
		goto out;

out:
	reftable_log_record_clear(&persisted);
	reftable_iterator_destroy(&iter);
	git__free(logs);
	return error;
}

static int refdb_reftable_reflog_rename(git_refdb_backend *_backend, const char *old_name, const char *new_name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_log_record old_log = {0}, updates[2] = {{0}, {0}};
	int error;

	if (!git_reference_is_valid_name(new_name))
		return GIT_EINVALIDSPEC;

	if ((error = reftable_stack_reload(backend->stack)) < 0 ||
	    (error = reftable_stack_read_log(backend->stack, old_name, &old_log)) < 0) {
		error = reftable_error(error, "could not read reflog for renaming");
		goto out;
	}

	updates[0].refname = (char *) old_name;
	updates[1] = old_log;
	updates[1].refname = (char *) new_name;

	if ((error = refdb_reftable_write_logs(backend, updates, 2)) < 0)
		goto out;

out:
	reftable_log_record_clear(&old_log);
	return error;
}

static int refdb_reftable_reflog_delete(git_refdb_backend *_backend, const char *name)
{
	GIT_UNUSED(_backend);
	GIT_UNUSED(name);
	return GIT_ENOTSUPP;
}

static int refdb_reftable_compress(git_refdb_backend *_backend)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	if (reftable_stack_reload(backend->stack) < 0)
		return 1;
	return reftable_stack_compact_all(backend->stack, NULL);
}

int git_refdb_backend_reftable(
	git_refdb_backend **out,
	git_repository *repository)
{
	struct reftable_write_options options = {0};
	struct reftable_stack *stack;
	git_buf dir = GIT_BUF_INIT;
	refdb_reftable *backend;
	int error;

	backend = git__calloc(1, sizeof(refdb_reftable));
	GIT_ERROR_CHECK_ALLOC(backend);

	if ((error = git_refdb_init_backend(&backend->parent, GIT_REFDB_BACKEND_VERSION)) < 0)
		goto out;
	backend->repo = repository;

	if ((error = git_buf_joinpath(&dir, git_repository_path(backend->repo), "reftable")) < 0 ||
	    (error = reftable_new_stack(&stack, dir.ptr, options)) < 0)
		goto out;
	backend->stack = stack;

	backend->parent.exists = refdb_reftable_exists;
	backend->parent.lookup = refdb_reftable_lookup;
	backend->parent.iterator = refdb_reftable_iterator_new;
	backend->parent.write = refdb_reftable_write;
	backend->parent.rename = refdb_reftable_rename;
	backend->parent.del = refdb_reftable_delete;
	backend->parent.has_log = refdb_reftable_has_log;
	backend->parent.ensure_log = refdb_reftable_ensure_log;
	backend->parent.free = refdb_reftable_free;
	backend->parent.reflog_read = refdb_reftable_reflog_read;
	backend->parent.reflog_write = refdb_reftable_reflog_write;
	backend->parent.reflog_rename = refdb_reftable_reflog_rename;
	backend->parent.reflog_delete = refdb_reftable_reflog_delete;
	backend->parent.compress = refdb_reftable_compress;
	/* TODO: transaction API */

	*out = (git_refdb_backend *)backend;
out:
	git_buf_dispose(&dir);
	return error;
}
