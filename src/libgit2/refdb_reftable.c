/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "refdb_reftable.h"

#include "refdb.h"
#include "reflog.h"
#include "signature.h"
#include "wildmatch.h"

#include <git2/object.h>
#include <git2/oid.h>
#include <git2/refdb.h>
#include <git2/refs.h>
#include <git2/sys/refdb_backend.h>
#include <git2/sys/refs.h>

#include <reftable-error.h>
#include <reftable-iterator.h>
#include <reftable-merged.h>
#include <reftable-record.h>
#include <reftable-stack.h>
#include <reftable-table.h>
#include <reftable-writer.h>

typedef enum {
	REFDB_REFTABLE_STACK_MAIN,
	REFDB_REFTABLE_STACK_WORKTREE,
} refdb_reftable_stack_t;

typedef struct {
	struct reftable_stack *stack;
	refdb_reftable_stack_t which;
} refdb_reftable_stack;

typedef struct {
	git_refdb_backend parent;
	git_repository *repo;
	refdb_reftable_stack *stack;
	refdb_reftable_stack *worktree_stack;
} refdb_reftable;

typedef struct {
	refdb_reftable_stack *stack;
	struct reftable_iterator iter;
	struct reftable_ref_record ref;
	enum {
		STACK_ITER_ADVANCE,
		STACK_ITER_READY,
		STACK_ITER_EXHAUSTED,
	} state;
} refdb_reftable_stack_iterator;

typedef struct {
	git_reference_iterator parent;
	refdb_reftable *backend;
	refdb_reftable_stack_iterator main;
	refdb_reftable_stack_iterator worktree;
	const char *glob;
} refdb_reftable_iterator;

static int refdb_reftable_error(int error, const char *msg)
{
	int class;

	switch ((enum reftable_error) error) {
	case REFTABLE_NOT_EXIST_ERROR:
		class = GIT_ENOTFOUND;
		break;
	case REFTABLE_LOCK_ERROR:
		class = GIT_ELOCKED;
		break;
	case REFTABLE_API_ERROR:
		class = GIT_EINVALID;
		break;
	case REFTABLE_REFNAME_ERROR:
		class = GIT_EINVALIDSPEC;
		break;
	case REFTABLE_OUTDATED_ERROR:
		class = GIT_EMODIFIED;
		break;
	default:
		class = GIT_ERROR;
		break;
	}

	git_error_set(GIT_ERROR_REFERENCE, "%s: %s",
		      msg, reftable_error_str(error));
	return class;
}

static void refdb_reftable_stack_free(void *_stack)
{
	refdb_reftable_stack *stack = _stack;
	if (stack) {
		reftable_stack_destroy(stack->stack);
		git__free(stack);
	}
}

static void refdb_reftable_return_stack(refdb_reftable *backend,
					refdb_reftable_stack *stack)
{
	if (!stack)
		return;

	switch (stack->which) {
	case REFDB_REFTABLE_STACK_WORKTREE:
		if (git_atomic_compare_and_swap(&backend->worktree_stack, NULL, stack) != NULL)
			refdb_reftable_stack_free(stack);
		break;
	case REFDB_REFTABLE_STACK_MAIN:
		if (git_atomic_compare_and_swap(&backend->stack, NULL, stack) != NULL)
			refdb_reftable_stack_free(stack);
		break;
	}
}

static int refdb_reftable_stack_for(refdb_reftable_stack **out,
				    refdb_reftable *backend, refdb_reftable_stack_t which)
{
	struct reftable_write_options options = { 0 };
	refdb_reftable_stack **stack_ptr, *stack;
	const char *parent_directory;
	git_str path = GIT_STR_INIT;
	int error;

	*out = NULL;

#ifdef GIT_EXPERIMENTAL_SHA256
	switch (backend->repo->oid_type) {
	case GIT_OID_SHA1:
		options.hash_id = REFTABLE_HASH_SHA1;
		break;
	case GIT_OID_SHA256:
		options.hash_id = REFTABLE_HASH_SHA256;
		break;
	default:
		error = GIT_EINVALID;
		goto out;
	}
#else
	options.hash_id = REFTABLE_HASH_SHA1;
#endif
	options.default_permissions = 0666;
	options.disable_auto_compact = 0;
	options.fsync = p_fsync;
	options.lock_timeout_ms = 100;

	switch (which) {
	case REFDB_REFTABLE_STACK_WORKTREE:
		if (git_repository_is_worktree(backend->repo)) {
			stack_ptr = &backend->worktree_stack;
			parent_directory = backend->repo->gitdir;
			break;
		}

		/*
		 * The worktree stack was requested, but we're not
		 * in a worktree.
		 */

		/* fallthru */
	case REFDB_REFTABLE_STACK_MAIN:
		stack_ptr = &backend->stack;
		parent_directory = backend->repo->commondir;
		break;
	default:
		error = -1;
		goto out;
	}

	stack = git_atomic_swap(*stack_ptr, NULL);
	if (stack) {
		if ((error = reftable_stack_reload(stack->stack)) < 0) {
			error = refdb_reftable_error(error, "failed reloading stack");
			goto out;
		}
	} else {
		stack = git__calloc(1, sizeof(*stack));
		GIT_ERROR_CHECK_ALLOC(stack);
		stack->which = which;

		if ((error = git_str_joinpath(&path, parent_directory, "reftable")) < 0 ||
		    (error = reftable_new_stack(&stack->stack, path.ptr, &options)) < 0) {
			refdb_reftable_stack_free(stack);
			goto out;
		}
	}

	*out = stack;
	error = 0;

out:
	git_str_dispose(&path);
	return error;
}

static bool is_per_worktree_ref(const char *ref_name)
{
	return git__prefixcmp(ref_name, "refs/") != 0 ||
	       git__prefixcmp(ref_name, "refs/bisect/") == 0 ||
	       git__prefixcmp(ref_name, "refs/worktree/") == 0 ||
	       git__prefixcmp(ref_name, "refs/rewritten/") == 0;
}

static int refdb_reftable_stack_for_refname(refdb_reftable_stack **out,
					    refdb_reftable *backend,
					    const char *refname)
{
	refdb_reftable_stack_t type = REFDB_REFTABLE_STACK_MAIN;
	if (is_per_worktree_ref(refname))
		type = REFDB_REFTABLE_STACK_WORKTREE;
	return refdb_reftable_stack_for(out, backend, type);
}

static int refdb_reftable_reference_from_record(git_reference **out,
						struct reftable_ref_record *record,
						git_oid_t type)
{
	git_reference *ref;
	int error = 0;

	switch (record->value_type) {
	case REFTABLE_REF_SYMREF:
		ref = git_reference__alloc_symbolic(record->refname, record->value.symref);
		break;
	case REFTABLE_REF_VAL1: {
		git_oid oid;
		if ((error = git_oid_from_raw(&oid, record->value.val1, type)) < 0)
			goto out;
		ref = git_reference__alloc(record->refname, &oid, NULL);
		break;
	}
	case REFTABLE_REF_VAL2: {
		git_oid oid, peeled;
		if ((error = git_oid_from_raw(&oid, record->value.val2.value, type)) < 0 ||
		    (error = git_oid_from_raw(&peeled, record->value.val2.target_value, type)) < 0)
			goto out;
		ref = git_reference__alloc(record->refname, &oid, &peeled);
		break;
	}
	default:
		error = -1;
		goto out;
	}

	GIT_ERROR_CHECK_ALLOC(ref);
	*out = ref;
out:
	return error;
}

static int refdb_reftable_check_refname_available(refdb_reftable_stack *stack,
						  const char *old_name,
						  const char *new_name,
						  int force)
{
	struct reftable_ref_record ref = { 0 };
	struct reftable_iterator iter = { 0 };
	struct reftable_merged_table *table;
	git_str buf = GIT_STR_INIT;
	int error;

	/*
	 * Check if the reference itself exists. If so, we only allow the
	 * update when forcing it.
	 */
	if ((error = reftable_stack_read_ref(stack->stack, new_name, &ref)) < 0) {
		error = refdb_reftable_error(error, "could not read ref for collision checks");
		goto out;
	} else if (error == 0) {
		if (!force) {
			git_error_set(GIT_ERROR_REFERENCE,
				      "failed to write reference '%s': a reference with "
				      "that name already exists.", new_name);
			error = GIT_EEXISTS;
		} else {
			error = 0;
		}

		goto out;
	}

	/*
	 * Otherwise, we need to check whether there are any references nested
	 * below the new name. E.g., there must not be two refs refs/heads/foo
	 * and refs/heads/foo/bar.
	 */
	table = reftable_stack_merged_table(stack->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	if ((error = git_str_printf(&buf, "%s/", new_name)) < 0 ||
	    (error = reftable_merged_table_init_ref_iterator(table, &iter)) < 0 ||
	    (error = reftable_iterator_seek_ref(&iter, buf.ptr)) < 0) {
		error = refdb_reftable_error(error, "could not check for nested conflicts");
		goto out;
	}

	while (1) {
		if ((error = reftable_iterator_next_ref(&iter, &ref)) < 0) {
			error = refdb_reftable_error(error, "could not check for nested conflicts");
			goto out;
		} else if (error > 0) {
			/* The iterator didn't yield any more refs, so we're good. */
			break;
		} else if (old_name && !git__strcmp(ref.refname, old_name)) {
			/*
			 * This is the ref we're about to rename, so this is
			 * fine. We need to check subsequent refs though in
			 * case those might conflict. We do have to check
			 * subsequent refs though, as there might be other
			 * nested refs that conflict.
			 */
			continue;
		} else if (git__strncmp(ref.refname, buf.ptr, buf.size)) {
			/*
			 * This reference does not match our prefix. We have
			 * thus exhausted the new refs' prefix and can stop
			 * searching for conflicts.
			 */
			break;
		} else {
			git_error_set(GIT_ERROR_REFERENCE,
				      "cannot lock ref '%s', there are refs beneath that folder", new_name);
			error = GIT_EDIRECTORY;
			goto out;
		}
	}

	/*
	 * And last we need to check that there are no prefixes. E.g., there
	 * must be no ref "refs/heads" when we create "refs/heads/branch".
	 */
	if ((error = git_str_sets(&buf, new_name)) < 0)
		goto out;
	while (strchr(buf.ptr, '/')) {
		git_str_rtruncate_at_char(&buf, '/');

		/*
		 * If this is the reference we're about to rename we can abort
		 * searching. We know that there cannot be any conflicting ref
		 * any further up the hierarchy, as otherwise the old ref could
		 * not have existed, either.
		 */
		if (old_name && !git__strcmp(buf.ptr, old_name))
			break;

		if ((error = reftable_stack_read_ref(stack->stack, buf.ptr, &ref)) < 0) {
			error = refdb_reftable_error(error, "could not read ref for collision checks");
			goto out;
		} else if (error == 0) {
			git_error_set(GIT_ERROR_REFERENCE,
				      "path to reference '%s' collides with existing one", new_name);
			error = -1;
			goto out;
		}
	}

	error = 0;
out:
	reftable_ref_record_release(&ref);
	reftable_iterator_destroy(&iter);
	git_str_dispose(&buf);
	return error;
}

static int refdb_reftable_check_ref(refdb_reftable_stack *stack,
				    const char *refname,
				    const git_oid *expected_oid,
				    const char *expected_target)
{
	struct reftable_ref_record ref = { 0 };
	int error;

	if (!expected_oid && !expected_target)
		return 0;

	if ((error = reftable_stack_read_ref(stack->stack, refname, &ref)) < 0) {
		error = refdb_reftable_error(error, "failed reading reference");
		goto out;
	} else if (error > 0 && expected_oid && git_oid_is_zero(expected_oid)) {
		error = 0;
		goto out;
	} else if (error > 0) {
		error = GIT_ENOTFOUND;
		goto out;
	}

	if (expected_oid && reftable_ref_record_val1(&ref) == NULL) {
		error = GIT_EMODIFIED;
		goto out;
	}

	if (expected_target && ref.value_type != REFTABLE_REF_SYMREF) {
		error = GIT_EMODIFIED;
		goto out;
	}

	if (expected_oid && reftable_ref_record_val1(&ref) != NULL) {
#ifdef GIT_EXPERIMENTAL_SHA256
		git_oid_t oid_type = expected_oid->type;
#else
		git_oid_t oid_type = GIT_OID_SHA1;
#endif
		git_oid oid;

		if ((error = git_oid_from_raw(&oid, reftable_ref_record_val1(&ref), oid_type)) < 0)
			goto out;
		if (!git_oid_equal(&oid, expected_oid)) {
			error = GIT_EMODIFIED;
			goto out;
		}
	}

	if (expected_target && ref.value_type == REFTABLE_REF_SYMREF) {
		if (git__strcmp(expected_target, ref.value.symref)) {
			error = GIT_EMODIFIED;
			goto out;
		}
	}

	error = 0;
out:
	reftable_ref_record_release(&ref);
	return error;
}

static int refdb_reftable_log_fill(struct reftable_log_record *out,
				   const git_signature *who,
				   const git_oid *old_id,
				   const git_oid *new_id,
				   const char *reference,
				   const char *message,
				   uint64_t update_index)
{
	memset(out, 0, sizeof(*out));
	out->refname = reference ? git__strdup(reference) : NULL;
	out->update_index = update_index;
	out->value_type = REFTABLE_LOG_UPDATE;
	if (who) {
		out->value.update.name = who->name ? git__strdup(who->name) : NULL;
		out->value.update.email = who->email ? git__strdup(who->email) : NULL;
		out->value.update.time = who->when.time;
		out->value.update.tz_offset = who->when.offset;
	}
	if (old_id)
		memcpy(out->value.update.old_hash, old_id->id, GIT_OID_MAX_SIZE);
	if (new_id)
		memcpy(out->value.update.new_hash, new_id->id, GIT_OID_MAX_SIZE);
	out->value.update.message = message ? git__strdup(message) : NULL;
	return 0;
}

typedef struct {
	const char *initial_head;
	int error;
} refdb_reftable_write_head_data;

static int refdb_reftable_write_head_table(struct reftable_writer *wr, void *cb_data)
{
	refdb_reftable_write_head_data *data = cb_data;
	struct reftable_ref_record head = { 0 };
	int error;

	head.refname = (char *) GIT_HEAD_REF;
	head.update_index = 1;
	head.value_type = REFTABLE_REF_SYMREF;
	head.value.symref = (char *) data->initial_head;

	if ((error = reftable_writer_set_limits(wr, 1, 1)) < 0 ||
	    (error = reftable_writer_add_refs(wr, &head, 1)) < 0) {
		data->error = refdb_reftable_error(error, "failed queueing initial head ref");
		goto out;
	}

out:
	return error;
}

static int refdb_reftable_init(git_refdb_backend *_backend,
			       const char *initial_head,
			       mode_t mode,
			       uint32_t flags)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_stack *stack = NULL;
	git_str path = GIT_STR_INIT;
	mode_t dmode;
	int error;

	if (mode == GIT_REPOSITORY_INIT_SHARED_UMASK)
		dmode = 0777;
	else if (mode == GIT_REPOSITORY_INIT_SHARED_GROUP)
		dmode = (0775 | S_ISGID);
	else if (mode == GIT_REPOSITORY_INIT_SHARED_ALL)
		dmode = (0777 | S_ISGID);
	else
		dmode = mode;

	if ((error = git_str_joinpath(&path, backend->repo->gitdir, "reftable")) < 0 ||
	    (error = git_futils_mkdir(path.ptr, dmode, 0) < 0))
		goto out;

	if (initial_head) {
		int write_head = 1;

		if ((error = refdb_reftable_stack_for_refname(&stack, backend, GIT_HEAD_REF)) < 0)
			goto out;

		if ((flags & GIT_REFDB_BACKEND_INIT_FORCE_HEAD) == 0) {
			struct reftable_ref_record existing_ref = { 0 };

			if ((error = reftable_stack_read_ref(stack->stack, GIT_HEAD_REF, &existing_ref)) < 0) {
				error = refdb_reftable_error(error, "failed reference lookup");
				goto out;
			}

			write_head = (error > 0);
		}

		if (write_head) {
			refdb_reftable_write_head_data data;

			data.initial_head = initial_head;
			data.error = 0;

			if ((error = reftable_stack_add(stack->stack, refdb_reftable_write_head_table,
							&data, REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
				if (data.error)
					error = data.error;
				else
					error = refdb_reftable_error(error, "failed stack update");
				goto out;
			}
		}
	}

out:
	refdb_reftable_return_stack(backend, stack);
	git_str_dispose(&path);
	return error;
}

static int refdb_reftable_exists(int *exists,
				 git_refdb_backend *_backend,
				 const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_stack *stack;
	struct reftable_ref_record ref = { 0 };
	int error;

	if ((error = refdb_reftable_stack_for_refname(&stack, backend, refname)) < 0)
		goto out;

	if ((error = reftable_stack_read_ref(stack->stack, refname, &ref)) < 0) {
		error = refdb_reftable_error(error, "failed reading reference");
		goto out;
	}

	*exists = (error == 0);

out:
	refdb_reftable_return_stack(backend, stack);
	reftable_ref_record_release(&ref);
	return error;
}

static int refdb_reftable_lookup(git_reference **out,
				 git_refdb_backend *_backend,
				 const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_stack *stack;
	struct reftable_ref_record ref = { 0 };
	int error;

	if ((error = refdb_reftable_stack_for_refname(&stack, backend, refname)) < 0)
		goto out;

	if ((error = reftable_stack_read_ref(stack->stack, refname, &ref)) < 0) {
		error = refdb_reftable_error(error, "failed reference lookup");
		goto out;
	} else if (error > 0) {
		git_error_set(GIT_ERROR_REFERENCE, "reference '%s' not found", refname);
		error = GIT_ENOTFOUND;
		goto out;
	}

	if ((error = refdb_reftable_reference_from_record(out, &ref, backend->repo->oid_type)) < 0)
		goto out;

out:
	refdb_reftable_return_stack(backend, stack);
	reftable_ref_record_release(&ref);
	return error;
}

static int refdb_reftable_stack_iter_maybe_advance(refdb_reftable_stack_iterator *it,
				    refdb_reftable_stack_t which,
				    const char *glob, bool is_worktree)
{
	if (it->state == STACK_ITER_READY ||
	    it->state == STACK_ITER_EXHAUSTED)
		return 0;

	while (1) {
		int error;

		if ((error = reftable_iterator_next_ref(&it->iter, &it->ref)) != 0) {
			if (error > 0) {
				it->state = STACK_ITER_EXHAUSTED;
				return 0;
			}
			return refdb_reftable_error(error, "failed retrieving next record");
		}

		switch (which) {
		case REFDB_REFTABLE_STACK_MAIN:
			if (is_worktree && is_per_worktree_ref(it->ref.refname))
				continue;
			break;
		case REFDB_REFTABLE_STACK_WORKTREE:
			if (!is_per_worktree_ref(it->ref.refname))
				continue;
			break;
		}

		if (glob && wildmatch(glob, it->ref.refname, 0) != 0)
			continue;

		it->state = STACK_ITER_READY;
		return 0;
	}
}

static int refdb_reftable_merged_iter_next(struct reftable_ref_record **out,
					   refdb_reftable_iterator *it)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(it->parent.db->backend, refdb_reftable, parent);
	bool is_worktree = git_repository_is_worktree(backend->repo);
	int error;

	if ((error = refdb_reftable_stack_iter_maybe_advance(&it->main, REFDB_REFTABLE_STACK_MAIN,
							     it->glob, is_worktree)) < 0)
		return error;

	if (git_repository_is_worktree(backend->repo)) {
		if ((error = refdb_reftable_stack_iter_maybe_advance(&it->worktree, REFDB_REFTABLE_STACK_WORKTREE,
								     it->glob, is_worktree)) < 0)
			return error;

		if (it->main.state == STACK_ITER_READY &&
		    it->worktree.state == STACK_ITER_READY) {
			int cmp = git__strcmp(it->main.ref.refname, it->worktree.ref.refname);
			if (cmp < 0)
				goto yield_main;
			else
				goto yield_worktree;
		} else if (it->main.state == STACK_ITER_READY) {
			goto yield_main;
		} else if (it->worktree.state == STACK_ITER_READY) {
			goto yield_worktree;
		} else {
			return GIT_ITEROVER;
		}
	} else {
		if (it->main.state == STACK_ITER_EXHAUSTED)
			return GIT_ITEROVER;
		goto yield_main;
	}

yield_main:
	it->main.state = STACK_ITER_ADVANCE;
	*out = &it->main.ref;
	return 0;

yield_worktree:
	it->worktree.state = STACK_ITER_ADVANCE;
	*out = &it->worktree.ref;
	return 0;
}

static int refdb_reftable_iterator_next(git_reference **out, git_reference_iterator *_it)
{
	refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
	refdb_reftable *backend = GIT_CONTAINER_OF(it->parent.db->backend, refdb_reftable, parent);
	struct reftable_ref_record *ref;
	int error;

	if ((error = refdb_reftable_merged_iter_next(&ref, it)) < 0 ||
	    (error = refdb_reftable_reference_from_record(out, ref, backend->repo->oid_type)) < 0)
		return error;

	return 0;
}

static int refdb_reftable_iterator_next_name(const char **out, git_reference_iterator *_it)
{
	refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
	struct reftable_ref_record *ref;
	int error;

	if ((error = refdb_reftable_merged_iter_next(&ref, it)) < 0)
		return error;
	*out = ref->refname;

	return 0;
}

static void refdb_reftable_iterator_free(git_reference_iterator *_it)
{
	if (_it) {
		refdb_reftable_iterator *it = GIT_CONTAINER_OF(_it, refdb_reftable_iterator, parent);
		reftable_iterator_destroy(&it->main.iter);
		reftable_iterator_destroy(&it->worktree.iter);
		reftable_ref_record_release(&it->main.ref);
		reftable_ref_record_release(&it->worktree.ref);
		refdb_reftable_return_stack(it->backend, it->main.stack);
		refdb_reftable_return_stack(it->backend, it->worktree.stack);
		git__free((char *) it->glob);
		git__free(it);
	}
}

static int refdb_reftable_iterator_new(git_reference_iterator **out,
				       git_refdb_backend *_backend,
				       const char *glob)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_iterator *it = NULL;
	char *needle = NULL;
	int error;

	it = git__calloc(1, sizeof(*it));
	GIT_ERROR_CHECK_ALLOC(it);
	it->backend = backend;
	it->parent.next = refdb_reftable_iterator_next;
	it->parent.next_name = refdb_reftable_iterator_next_name;
	it->parent.free = refdb_reftable_iterator_free;

	if ((error = refdb_reftable_stack_for(&it->main.stack, backend, REFDB_REFTABLE_STACK_MAIN)) < 0 ||
	    (error = reftable_merged_table_init_ref_iterator(reftable_stack_merged_table(it->main.stack->stack),
							     &it->main.iter) < 0))
		goto out;

	if (git_repository_is_worktree(backend->repo)) {
		if ((error = refdb_reftable_stack_for(&it->worktree.stack, backend, REFDB_REFTABLE_STACK_WORKTREE)) < 0 ||
		    (error = reftable_merged_table_init_ref_iterator(reftable_stack_merged_table(it->worktree.stack->stack),
								     &it->worktree.iter) < 0))
			goto out;
	}

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
			git_str pattern = GIT_STR_INIT;
			if ((git_str_printf(&pattern, "refs/%s", glob)) < 0)
				goto out;
			it->glob = git_str_detach(&pattern);
		}

		needle = git__strndup(glob, glob - pos);
	} else {
		it->glob = git__strdup("refs/*");
		needle = git__strdup("refs/");
	}
	GIT_ERROR_CHECK_ALLOC(it->glob);
	GIT_ERROR_CHECK_ALLOC(needle);

	if ((error = reftable_iterator_seek_ref(&it->main.iter, needle)) < 0) {
		error = refdb_reftable_error(error, "failed updating reftable for update");
		goto out;
	}

	if (git_repository_is_worktree(backend->repo)) {
		if ((error = reftable_iterator_seek_ref(&it->worktree.iter, needle)) < 0) {
			error = refdb_reftable_error(error, "failed updating reftable for update");
			goto out;
		}
	}

	*out = &it->parent;
out:
	if (error < 0 && it)
		refdb_reftable_iterator_free(&it->parent);
	git__free(needle);
	return 0;
}

typedef struct {
	refdb_reftable *backend;
	refdb_reftable_stack *stack;
	const git_reference *ref;
	int force;
	const git_signature *who;
	const char *message;
	const git_oid *expected_oid;
	const char *expected_target;
	int error;
} refdb_reftable_write_table_data;

static int refdb_reftable_write_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_write_table_data *data = cb_data;
	struct reftable_log_record log_records[2] = {{ 0 }};
	struct reftable_ref_record ref_record = { 0 };
	const char *new_target = NULL;
	const git_oid *new_id = NULL;
	int error, write_reflog;
	uint64_t update_index;
	size_t logs_nr = 0, i;
	git_refdb *refdb;

	if (data->ref->type == GIT_REFERENCE_SYMBOLIC)
		new_target = data->ref->target.symbolic;
	else
		new_id = &data->ref->target.oid;

	/*
	 * Verify that the current state of the refname matches the expected
	 * state for non-racy updates.
	 */
	if ((error = refdb_reftable_check_ref(data->stack, data->ref->name, data->expected_oid, data->expected_target)) < 0) {
		data->error = error;
		git_error_set(GIT_ERROR_REFERENCE, "old reference value does not match");
		goto out;
	}

	if ((error = refdb_reftable_check_refname_available(data->stack, NULL, data->ref->name, data->force)) < 0) {
		data->error = error;
		goto out;
	}

	/*
	 * Check whether the update is a no-op. If so, we want to skip the
	 * update completely, most importantly so that we don't write a reflog
	 * entry.
	 */
	if ((error = refdb_reftable_check_ref(data->stack, data->ref->name, new_id, new_target)) < 0) {
		if (error == GIT_EMODIFIED) {
			/*
			 * The reference is different than what we expected.
			 * Good, proceed with updating it.
			 */
		} else if (error == GIT_ENOTFOUND && new_id && git_oid_is_zero(new_id)) {
			/*
			 * The reference does not exist, and we are about to
			 * delete it. As the current state already matches the
			 * desired state we don't have to do anything.
			 */
			error = 0;
			goto out;
		} else if (error == GIT_ENOTFOUND) {
			/*
			 * The reference does not exist, but we want it to.
			 * Good, continue with the write.
			 */
		} else {
			data->error = error;
			goto out;
		}
	} else {
		/*
		 * The reference already matches our desired value, so we do
		 * not need to write anything.
		 */
		error = 0;
		goto out;
	}

	update_index = reftable_stack_next_update_index(data->stack->stack);

	ref_record.refname = (char *) data->ref->name;
	ref_record.update_index = update_index;
	switch (git_reference_type(data->ref)) {
	case GIT_REFERENCE_SYMBOLIC:
		ref_record.value_type = REFTABLE_REF_SYMREF;
		ref_record.value.symref = (char *) git_reference_symbolic_target(data->ref);
		break;
	case GIT_REFERENCE_DIRECT: {
		git_object *peeled = NULL;

		if ((error = git_reference_peel(&peeled, data->ref, GIT_OBJECT_COMMIT)) == 0 &&
		    !git_oid_equal(git_reference_target(data->ref), git_object_id(peeled))) {
			ref_record.value_type = REFTABLE_REF_VAL2;
			memcpy(ref_record.value.val2.value, git_reference_target(data->ref), GIT_OID_MAX_SIZE);
			memcpy(ref_record.value.val2.target_value, git_object_id(peeled)->id, GIT_OID_MAX_SIZE);
		} else {
			ref_record.value_type = REFTABLE_REF_VAL1;
			memcpy(ref_record.value.val1, git_reference_target(data->ref), GIT_OID_MAX_SIZE);
		}

		git_object_free(peeled);
		break;
	}
	default:
		data->error = error = -1;
		goto out;
	}

	if ((error = git_repository_refdb__weakptr(&refdb, data->backend->repo)) < 0 ||
	    (error = git_refdb_should_write_reflog(&write_reflog, refdb, data->ref)) < 0) {
		data->error = error;
		goto out;
	}

	if (write_reflog) {
		int write_head_reflog = 0;
		git_oid old_id, new_id;

		git_oid_clear(&old_id, data->backend->repo->oid_type);
		git_oid_clear(&new_id, data->backend->repo->oid_type);

		error = git_reference_name_to_id(&old_id, data->backend->repo, data->ref->name);
		if (error < 0 && error != GIT_ENOTFOUND) {
			data->error = error;
			goto out;
		}

		if (data->ref->type == GIT_REFERENCE_SYMBOLIC) {
			error = git_reference_name_to_id(&new_id, data->backend->repo,
							 git_reference_symbolic_target(data->ref));
			if (error < 0 && error != GIT_ENOTFOUND) {
				data->error = error;
				goto out;
			}

			/* Detaching HEAD does not create an entry. */
			if (!strcmp(data->ref->name, GIT_HEAD_REF) && error == GIT_ENOTFOUND)
				write_reflog = 0;
			/* Symbolic refs other than HEAD do not create an entry, either. */
			else if (strcmp(data->ref->name, GIT_HEAD_REF))
				write_reflog = 0;
		} else {
			git_oid_cpy(&new_id, git_reference_target(data->ref));
		}

		if (write_reflog &&
		    (error = git_refdb_should_write_head_reflog(&write_head_reflog, refdb, data->ref)) < 0) {
			data->error = error;
			goto out;
		}

		if (write_reflog &&
		    (error = refdb_reftable_log_fill(&log_records[logs_nr++], data->who, &old_id, &new_id,
						     data->ref->name, data->message, update_index)) < 0) {
			data->error = error;
			goto out;
		}

		if (write_head_reflog &&
		    (error = refdb_reftable_log_fill(&log_records[logs_nr++], data->who, &old_id, &new_id,
						     GIT_HEAD_REF, data->message, update_index)) < 0) {
			data->error = error;
			goto out;
		}
	}

	if ((error = reftable_writer_set_limits(writer, update_index, update_index)) < 0 ||
	    (error = reftable_writer_add_refs(writer, &ref_record, 1)) < 0 ||
	    (error = reftable_writer_add_logs(writer, log_records, logs_nr)) < 0) {
		data->error = refdb_reftable_error(error, "failed writing update table");
		goto out;
	}

out:
	for (i = 0; i < logs_nr; i++)
		reftable_log_record_release(&log_records[i]);
	return error;
}

static int refdb_reftable_write(git_refdb_backend *_backend,
				const git_reference *ref,
				int force,
				const git_signature *who,
				const char *message,
				const git_oid *expected_oid,
				const char *expected_target)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_write_table_data data;
	int error;

	data.backend = backend;
	data.ref = ref;
	data.force = force;
	data.who = who;
	data.message = message;
	data.expected_oid = expected_oid;
	data.expected_target = expected_target;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, ref->name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_write_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

static int refdb_reftable_updates_for_reflog_delete_or_rename(refdb_reftable_stack *stack,
							      const char *old_name,
							      const char *new_name,
							      struct reftable_log_record **out,
							      size_t *out_nr)
{
	struct reftable_log_record *updates = NULL;
	struct reftable_log_record old_log = { 0 };
	struct reftable_iterator iter = { 0 };
	struct reftable_merged_table *table;
	size_t updates_nr = 0;
	int error;

	if (new_name) {
		int valid;

		if ((error = git_reference_name_is_valid(&valid, new_name)) < 0)
			goto out;
		if (!valid) {
			error = GIT_EINVALIDSPEC;
			goto out;
		}
	}

	table = reftable_stack_merged_table(stack->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	if ((error = reftable_merged_table_init_log_iterator(table, &iter)) < 0 ||
	    (error = reftable_iterator_seek_log(&iter, old_name)) < 0) {
		error = refdb_reftable_error(error, "could not get old reflog entries");
		goto out;
	}

	/*
	 * Deletion of reflogs means that we have to delete each reflog entry
	 * individually. If we want to rename, we have to also create the new
	 * entry at the same point in time.
	 */
	while (1) {
		struct reftable_log_record deletion = { 0 }, creation = { 0 };

		if ((error = reftable_iterator_next_log(&iter, &old_log)) < 0) {
			error = refdb_reftable_error(error, "could not get old reflog entry");
			goto out;
		}
		if (error > 0 || strcmp(old_log.refname, old_name))
			break;

		deletion.refname = git__strdup(old_name);
		deletion.value_type = REFTABLE_LOG_DELETION;
		deletion.update_index = old_log.update_index;

		if (new_name) {
			creation = old_log;
			git__free(creation.refname);
			creation.refname = git__strdup(new_name);
			memset(&old_log, 0, sizeof(old_log));
		}

		updates = git__reallocarray(updates, updates_nr + 1 + !!new_name, sizeof(*updates));
		updates[updates_nr++] = deletion;
		if (new_name)
			updates[updates_nr++] = creation;
	}

	*out = updates;
	*out_nr = updates_nr;
	updates = NULL;
	error = 0;

out:
	reftable_log_record_release(&old_log);
	reftable_iterator_destroy(&iter);
	git__free(updates);
	return error;
}

typedef struct {
	refdb_reftable_stack *stack;
	const char *refname;
	const git_oid *old_id;
	const char *old_target;
	int error;
} refdb_reftable_delete_data;

static int refdb_reftable_write_delete_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_delete_data *data = cb_data;
	struct reftable_log_record *log_deletions = NULL;
	struct reftable_ref_record ref = { 0 };
	size_t log_deletions_nr = 0, i;
	int error;

	if ((error = refdb_reftable_check_ref(data->stack, data->refname,
					      data->old_id, data->old_target)) < 0) {
		data->error = error;
		goto out;
	}

	ref.refname = (char *) data->refname;
	ref.update_index = reftable_stack_next_update_index(data->stack->stack);
	ref.value_type = REFTABLE_REF_DELETION;

	if ((error = refdb_reftable_updates_for_reflog_delete_or_rename(data->stack, data->refname, NULL,
									&log_deletions, &log_deletions_nr)) < 0) {
		data->error = error;
		goto out;
	}

	if ((error = reftable_writer_set_limits(writer, ref.update_index, ref.update_index)) < 0 ||
	    (error = reftable_writer_add_refs(writer, &ref, 1)) < 0 ||
	    (error = reftable_writer_add_logs(writer, log_deletions, log_deletions_nr)) < 0) {
		data->error = refdb_reftable_error(error, "failed writing ref deletions");
		goto out;
	}

out:
	for (i = 0; i < log_deletions_nr; i++)
		reftable_log_record_release(&log_deletions[i]);
	git__free(log_deletions);
	return error;
}

static int refdb_reftable_delete(git_refdb_backend *_backend,
				 const char *refname,
				 const git_oid *old_id,
				 const char *old_target)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_delete_data data;
	int error;

	data.refname = refname;
	data.old_id = old_id;
	data.old_target = old_target;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, refname)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_write_delete_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

typedef struct {
	refdb_reftable *backend;
	refdb_reftable_stack *stack;
	const char *old_name;
	const char *new_name;
	int force;
	const git_signature *who;
	const char *message;
	git_reference **out;
	int error;
} refdb_reftable_rename_data;

static int refdb_reftable_write_rename_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_rename_data *data = cb_data;
	struct reftable_ref_record refs[2] = {{ 0 }}, existing = { 0 }, renamed = { 0 };
	struct reftable_log_record *logs = NULL;
	uint64_t update_index;
	size_t logs_nr = 0, i;
	int error;

	if ((error = reftable_stack_read_ref(data->stack->stack, data->old_name, &existing)) < 0) {
		data->error = refdb_reftable_error(error, "failed reading reference to be renamed");
		goto out;
	} else if (error > 0) {
		data->error = GIT_ENOTFOUND;
		goto out;
	}

	if ((error = refdb_reftable_check_refname_available(data->stack, data->old_name,
							    data->new_name, data->force)) < 0) {
		data->error = error;
		goto out;
	}

	update_index = reftable_stack_next_update_index(data->stack->stack);

	if ((error = refdb_reftable_updates_for_reflog_delete_or_rename(data->stack, data->old_name,
									data->new_name, &logs, &logs_nr)) < 0) {
		data->error = error;
		goto out;
	}

	if (logs_nr) {
		git_oid oid;

		switch (existing.value_type) {
		case REFTABLE_REF_SYMREF:
			if ((error = git_reference_name_to_id(&oid, data->backend->repo,
							      existing.value.symref)) < 0) {
				if (error != GIT_ENOTFOUND) {
					data->error = error;
					goto out;
				}
				goto skip_log;
			}
			break;
		case REFTABLE_REF_VAL1:
		case REFTABLE_REF_VAL2:
			if ((error = git_oid_from_raw(&oid, reftable_ref_record_val1(&existing),
						      data->backend->repo->oid_type)) < 0) {
				data->error = error;
				goto out;
			}
			break;
		default:
			data->error = error = -1;
			goto out;
		}

		logs = git__reallocarray(logs, logs_nr + 1, sizeof(*logs));
		if ((error = refdb_reftable_log_fill(&logs[logs_nr], data->who, &oid,
						     &oid, data->new_name, data->message,
						     update_index)) < 0) {
			data->error = error;
			goto out;
		}
		logs_nr++;
	}

skip_log:
	refs[0].refname = (char *) data->old_name;
	refs[0].update_index = update_index;
	refs[0].value_type = REFTABLE_REF_DELETION;
	refs[1].refname = (char *) data->new_name;
	refs[1].update_index = update_index;
	refs[1].value = existing.value;
	refs[1].value_type = existing.value_type;

	/* Copy new record as the reftable library may sort it away under our feet. */
	renamed = refs[1];

	if ((error = reftable_writer_set_limits(writer, update_index, update_index)) < 0 ||
	    (error = reftable_writer_add_refs(writer, refs, 2)) < 0 ||
	    (error = reftable_writer_add_logs(writer, logs, logs_nr)) < 0) {
		data->error = refdb_reftable_error(error, "failed writing rename");
		goto out;
	}

	if ((error = refdb_reftable_reference_from_record(data->out, &renamed, data->backend->repo->oid_type)) < 0) {
		data->error = error;
		goto out;
	}

out:
	reftable_ref_record_release(&existing);
	for (i = 0; i < logs_nr; i++)
		reftable_log_record_release(&logs[i]);
	git__free(logs);
	return error;
}

static int refdb_reftable_rename(git_reference **out,
				 git_refdb_backend *_backend,
				 const char *old_name,
				 const char *new_name,
				 int force,
				 const git_signature *who,
				 const char *message)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_rename_data data;
	int error;

	data.backend = backend;
	data.old_name = old_name;
	data.new_name = new_name;
	data.force = force;
	data.who = who;
	data.message = message;
	data.out = out;
	data.error = 0;

	/* We do not (yet?) support renames across different worktree stacks. */
	if (git_repository_is_worktree(backend->repo) &&
	    is_per_worktree_ref(old_name) != is_per_worktree_ref(new_name)) {
		error = GIT_EINVALID;
		goto out;
	}

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, old_name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_write_rename_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

static void refdb_reftable_free(git_refdb_backend *_backend)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_stack_free(backend->worktree_stack);
	refdb_reftable_stack_free(backend->stack);
	git__free(backend);
}

static int refdb_reftable_has_log(git_refdb_backend *_backend, const char *refname)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_log_record record = { 0 };
	refdb_reftable_stack *stack;
	int error;

	if ((error = refdb_reftable_stack_for_refname(&stack, backend, refname)) < 0)
		goto out;

	if ((error = reftable_stack_read_log(stack->stack, refname, &record)) < 0) {
		error = refdb_reftable_error(error, "failed reading log record");
		goto out;
	}

	error = (error == 0);

out:
	reftable_log_record_release(&record);
	refdb_reftable_return_stack(backend, stack);
	return error;
}

typedef struct {
	refdb_reftable_stack *stack;
	const char *name;
	int error;
} refdb_reftable_ensure_log_data;

static int refdb_reftable_write_log_existence_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_ensure_log_data *data = cb_data;
	struct reftable_log_record log = { 0 };
	int error;

	if ((error = reftable_stack_read_log(data->stack->stack, data->name, &log)) < 0) {
		data->error = refdb_reftable_error(error, "failed reading log record");
		goto out;
	} else if (error > 0) {
		/* The log exists already, there's no need to write a new marker. */
		error = 0;
		goto out;
	}

	log.refname = (char *)data->name;
	log.update_index = reftable_stack_next_update_index(data->stack->stack);
	log.value_type = REFTABLE_LOG_UPDATE;

	/*
	 * The reftable format encodes an empty reflog by setting both old and
	 * new object ID to the null object ID. These entries will not be
	 * yielded by our reader, but can be used to verify that the reflog
	 * exists.
	 */
	if ((error = reftable_writer_set_limits(writer, log.update_index, log.update_index)) < 0 ||
	    (error = reftable_writer_add_logs(writer, &log, 1)) < 0) {
		data->error = refdb_reftable_error(error, "writing reflog ensistence marker");
		goto out;
	}

out:
	return error;
}

static int refdb_reftable_ensure_log(git_refdb_backend *_backend,
				     const char *name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_ensure_log_data data;
	int error;

	data.name = name;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_write_log_existence_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

static int refdb_reftable_reflog_read(git_reflog **out,
				      git_refdb_backend *_backend,
				      const char *name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	struct reftable_merged_table *table = NULL;
	struct reftable_log_record record = { 0 };
	struct reftable_iterator iter = { 0 };
	refdb_reftable_stack *stack;
	git_reflog *reflog = NULL;
	int error;

	if ((error = refdb_reftable_stack_for_refname(&stack, backend, name)) < 0)
		goto out;

	reflog = git__calloc(1, sizeof(git_reflog));
	GIT_ERROR_CHECK_ALLOC(reflog);
	reflog->ref_name = git__strdup(name);
	GIT_ERROR_CHECK_ALLOC(reflog->ref_name);
	reflog->oid_type = backend->repo->oid_type;

	if ((error = git_vector_init(&reflog->entries, 0, NULL)) < 0)
		goto out;

	table = reftable_stack_merged_table(stack->stack);
	GIT_ERROR_CHECK_ALLOC(table);

	if ((error = reftable_merged_table_init_log_iterator(table, &iter) < 0) ||
	    (error = reftable_iterator_seek_log(&iter, name)) < 0) {
		error = refdb_reftable_error(error, "could not get reflog entries");
		goto out;
	}

	while (1) {
		git_signature *signature;
		git_reflog_entry *entry;

		if ((error = reftable_iterator_next_log(&iter, &record)) < 0) {
			error = refdb_reftable_error(error, "could not get next reflog entry");
			goto out;
		}
		if (error > 0 || git__strcmp(record.refname, name))
			break;

		if ((error = git_signature_new(&signature,
					       record.value.update.name,
					       record.value.update.email,
					       record.value.update.time,
					       record.value.update.tz_offset)) < 0)
			continue;

		entry = git__calloc(1, sizeof(*entry));
		GIT_ERROR_CHECK_ALLOC(entry);
		entry->committer = signature;

		/* Compatibility hacks with the file-based reflog implementation. */
		if (record.value.update.message && record.value.update.message[0] == '\0') {
			git__free(record.value.update.message);
		} else if (record.value.update.message) {
			size_t len = strlen(record.value.update.message);
			while (len) {
				if (!git__isspace(record.value.update.message[len - 1]))
					break;
				len--;
			}
			if (len)
				entry->msg = git__strndup(record.value.update.message, len);
		}

		if ((error = git_oid_from_raw(&entry->oid_old, record.value.update.old_hash,
					      backend->repo->oid_type)) < 0 ||
		    (error = git_oid_from_raw(&entry->oid_cur, record.value.update.new_hash,
					      backend->repo->oid_type)) < 0)
			goto out;

		if (git_oid_is_zero(&entry->oid_old) && git_oid_is_zero(&entry->oid_cur)) {
			git_reflog_entry__free(entry);
			continue;
		}

		if ((git_vector_insert(&reflog->entries, entry)) < 0)
			goto out;
	}
	error = 0;

	/* Logs are expected in recency-order. */
	git_vector_reverse(&reflog->entries);

	*out = reflog;
	reflog = NULL;
out:
	reftable_log_record_release(&record);
	reftable_iterator_destroy(&iter);
	refdb_reftable_return_stack(backend, stack);
	git_reflog_free(reflog);
	return error;
}

typedef struct {
	refdb_reftable_stack *stack;
	git_reflog *reflog;
	int error;
} refdb_reftable_write_reflog_data;

static int refdb_reftable_write_reflog_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_write_reflog_data *data = cb_data;
	struct reftable_log_record *updates = NULL;
	size_t reflog_entries, updates_nr = 0, i;
	uint64_t min_update_index, update_index;
	int error;

	/*
	 * We perform this operation by first deleting all existing reflog
	 * entries and then recreating the new ones. This may be highly
	 * suboptimal in the case where the reflog only has a couple of new
	 * entries. But right now the data structure doesn't provide enough
	 * information to tell which reflog entries need to be appended.
	 */
	if ((error = refdb_reftable_updates_for_reflog_delete_or_rename(data->stack, data->reflog->ref_name,
									NULL, &updates, &updates_nr)) < 0) {
		data->error = error;
		goto out;
	}

	if (updates_nr) {
		update_index = 0;
		min_update_index = UINT64_MAX;

		for (i = 0; i < updates_nr; i++) {
			if (updates[i].update_index > update_index)
				update_index = updates[i].update_index;
		}

		update_index++;
	} else {
		update_index = reftable_stack_next_update_index(data->stack->stack);
		min_update_index = reftable_stack_next_update_index(data->stack->stack);
	}

	reflog_entries = git_reflog_entrycount(data->reflog);
	updates = git__reallocarray(updates, updates_nr + reflog_entries, sizeof(*updates));

	for (i = 0; i < reflog_entries; i++) {
		const git_reflog_entry *entry;

		if ((entry = git_reflog_entry_byindex(data->reflog, reflog_entries - i - 1)) == NULL) {
			data->error = error = -1;
			goto out;
		}

		if ((error = refdb_reftable_log_fill(&updates[updates_nr++],
						     entry->committer, &entry->oid_old,
						     &entry->oid_cur, data->reflog->ref_name,
						     entry->msg, update_index++)) < 0) {
			data->error = error;
			goto out;
		}
	}

	if ((error = reftable_writer_set_limits(writer, min_update_index, update_index)) < 0 ||
	    (error = reftable_writer_add_logs(writer, updates, updates_nr)) < 0) {
		data->error = refdb_reftable_error(error, "failed writing reflog records");
		goto out;
	}

out:
	for (i = 0; i < updates_nr; i++)
		reftable_log_record_release(&updates[i]);
	git__free(updates);
	return error;
}

static int refdb_reftable_reflog_write(git_refdb_backend *_backend, git_reflog *reflog)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_write_reflog_data data;
	int error;

	data.reflog = reflog;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, reflog->ref_name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_write_reflog_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

typedef struct {
	refdb_reftable_stack *stack;
	const char *old_name;
	const char *new_name;
	int error;
} refdb_reftable_reflog_rename_data;

static int refdb_reftable_reflog_write_rename_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_reflog_rename_data *data = cb_data;
	struct reftable_log_record *updates = NULL;
	size_t updates_nr = 0, i;
	int error;

	if ((error = refdb_reftable_updates_for_reflog_delete_or_rename(data->stack, data->old_name,
									data->new_name, &updates, &updates_nr)) < 0) {
		data->error = error;
		goto out;
	}

	if ((error = reftable_writer_set_limits(writer, updates[0].update_index, updates[0].update_index)) < 0 ||
	    (error = reftable_writer_add_logs(writer, updates, updates_nr)) < 0) {
		data->error = refdb_reftable_error(error, "writing rename log records");
		goto out;
	}

out:
	for (i = 0; i < updates_nr; i++)
		reftable_log_record_release(&updates[i]);
	git__free(updates);
	return error;
}

static int refdb_reftable_reflog_rename(git_refdb_backend *_backend, const char *old_name, const char *new_name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_reflog_rename_data data;
	int error;

	/* We do not (yet?) support renames across different worktree stacks. */
	if (git_repository_is_worktree(backend->repo) &&
	    is_per_worktree_ref(old_name) != is_per_worktree_ref(new_name)) {
		error = GIT_EINVALID;
		goto out;
	}

	data.old_name = old_name;
	data.new_name = new_name;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, old_name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_reflog_write_rename_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

typedef struct {
	refdb_reftable_stack *stack;
	const char *name;
	int error;
} refdb_reftable_reflog_delete_data;

static int refdb_reftable_reflog_write_delete_table(struct reftable_writer *writer, void *cb_data)
{
	refdb_reftable_reflog_delete_data *data = cb_data;
	struct reftable_log_record *deletions = NULL;
	size_t deletions_nr;
	int error;

	if ((error = refdb_reftable_updates_for_reflog_delete_or_rename(data->stack, data->name, NULL,
									&deletions, &deletions_nr)) < 0) {
		data->error = error;
		goto out;
	}

	if ((error = reftable_writer_set_limits(writer, deletions[0].update_index, deletions[0].update_index)) < 0 ||
	    (error = reftable_writer_add_logs(writer, deletions, deletions_nr)) < 0) {
		data->error = refdb_reftable_error(error, "writing reflog deletion records");
		goto out;
	}

out:
	git__free(deletions);
	return error;
}

static int refdb_reftable_reflog_delete(git_refdb_backend *_backend, const char *name)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_reflog_delete_data data;
	int error;

	data.name = name;
	data.error = 0;

	if ((error = refdb_reftable_stack_for_refname(&data.stack, backend, name)) < 0)
		goto out;

	if ((error = reftable_stack_add(data.stack->stack, refdb_reftable_reflog_write_delete_table, &data,
					REFTABLE_STACK_NEW_ADDITION_RELOAD)) < 0) {
		if (data.error)
			error = data.error;
		else
			error = refdb_reftable_error(error, "failed stack update");
		goto out;
	}

out:
	refdb_reftable_return_stack(backend, data.stack);
	return error;
}

static int refdb_reftable_compress(git_refdb_backend *_backend)
{
	refdb_reftable *backend = GIT_CONTAINER_OF(_backend, refdb_reftable, parent);
	refdb_reftable_stack *stack = NULL, *wt_stack = NULL;
	int error;

	if ((error = refdb_reftable_stack_for(&stack, backend, REFDB_REFTABLE_STACK_MAIN)) < 0)
		goto out;

	if ((error = reftable_stack_compact_all(stack->stack, NULL)) < 0) {
		error = refdb_reftable_error(error, "could not compact stack");
		goto out;
	}

	if (git_repository_is_worktree(backend->repo)) {
		if ((error = refdb_reftable_stack_for(&wt_stack, backend,
						      REFDB_REFTABLE_STACK_WORKTREE)) < 0)
			goto out;

		if ((error = reftable_stack_compact_all(wt_stack->stack, NULL)) < 0) {
			error = refdb_reftable_error(error, "could not compact worktree stack");
			goto out;
		}
	}

out:
	refdb_reftable_return_stack(backend, wt_stack);
	refdb_reftable_return_stack(backend, stack);
	return error;
}

int git_refdb_backend_reftable(git_refdb_backend **out,
			       git_repository *repository)
{
	git_str dir = GIT_STR_INIT;
	refdb_reftable *backend;
	int error;

	/*
	 * TODO: this backend does not yet have support for namespaces. So if
	 * we see a repository with a namespace enabled we error out.
	 */
	if (repository->namespace) {
		git_error_set(GIT_ERROR_REFERENCE,
			      "reftable backend does not support namespaces");
		error = GIT_ENOTSUPPORTED;
		goto out;
	}

	backend = git__calloc(1, sizeof(refdb_reftable));
	GIT_ERROR_CHECK_ALLOC(backend);

	if ((error = git_refdb_init_backend(&backend->parent, GIT_REFDB_BACKEND_VERSION)) < 0)
		goto out;

	backend->repo = repository;
	backend->parent.init = refdb_reftable_init;
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
	git_str_dispose(&dir);
	return error;
}

static void *reftable_git_malloc(size_t size)
{
	return git__allocator.gmalloc(size, __FILE__, __LINE__);
}

static void *reftable_git_realloc(void *ptr, size_t new_size)
{
	return git__allocator.grealloc(ptr, new_size, __FILE__, __LINE__);
}

static void reftable_git_free(void *ptr)
{
	git__allocator.gfree(ptr);
}

int git_reftable_global_init(void)
{
	reftable_set_alloc(reftable_git_malloc,
			   reftable_git_realloc,
			   reftable_git_free);
	return 0;
}
