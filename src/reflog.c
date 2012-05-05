/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "reflog.h"
#include "repository.h"
#include "filebuf.h"
#include "signature.h"

static int reflog_init(git_reflog **reflog, git_reference *ref)
{
	git_reflog *log;

	*reflog = NULL;

	log = git__calloc(1, sizeof(git_reflog));
	GITERR_CHECK_ALLOC(log);

	log->ref_name = git__strdup(ref->name);
	GITERR_CHECK_ALLOC(log->ref_name);

	if (git_vector_init(&log->entries, 0, NULL) < 0) {
		git__free(log->ref_name);
		git__free(log);
		return -1;
	}

	*reflog = log;

	return 0;
}

static int reflog_write(const char *log_path, const char *oid_old,
			const char *oid_new, const git_signature *committer,
			const char *msg)
{
	int error;
	git_buf log = GIT_BUF_INIT;
	git_filebuf fbuf = GIT_FILEBUF_INIT;
	bool trailing_newline = false;

	assert(log_path && oid_old && oid_new && committer);

	if (msg) {
		const char *newline = strchr(msg, '\n');
		if (newline) {
			if (*(newline + 1) == '\0')
				trailing_newline = true;
			else {
				giterr_set(GITERR_INVALID, "Reflog message cannot contain newline");
				return -1;
			}
		}
	}

	git_buf_puts(&log, oid_old);
	git_buf_putc(&log, ' ');

	git_buf_puts(&log, oid_new);

	git_signature__writebuf(&log, " ", committer);
	git_buf_truncate(&log, log.size - 1); /* drop LF */

	if (msg) {
		git_buf_putc(&log, '\t');
		git_buf_puts(&log, msg);
	}

	if (!trailing_newline)
		git_buf_putc(&log, '\n');

	if (git_buf_oom(&log)) {
		git_buf_free(&log);
		return -1;
	}

	error = git_filebuf_open(&fbuf, log_path, GIT_FILEBUF_APPEND);
	if (!error) {
		if ((error = git_filebuf_write(&fbuf, log.ptr, log.size)) < 0)
			git_filebuf_cleanup(&fbuf);
		else
			error = git_filebuf_commit(&fbuf, GIT_REFLOG_FILE_MODE);
	}

	git_buf_free(&log);

	return error;
}

static int reflog_parse(git_reflog *log, const char *buf, size_t buf_size)
{
	const char *ptr;
	git_reflog_entry *entry;

#define seek_forward(_increase) do { \
	if (_increase >= buf_size) { \
		giterr_set(GITERR_INVALID, "Ran out of data while parsing reflog"); \
		goto fail; \
	} \
	buf += _increase; \
	buf_size -= _increase; \
	} while (0)

	while (buf_size > GIT_REFLOG_SIZE_MIN) {
		entry = git__malloc(sizeof(git_reflog_entry));
		GITERR_CHECK_ALLOC(entry);

		entry->committer = git__malloc(sizeof(git_signature));
		GITERR_CHECK_ALLOC(entry->committer);

		if (git_oid_fromstrn(&entry->oid_old, buf, GIT_OID_HEXSZ) < 0)
			goto fail;
		seek_forward(GIT_OID_HEXSZ + 1);

		if (git_oid_fromstrn(&entry->oid_cur, buf, GIT_OID_HEXSZ) < 0)
			goto fail;
		seek_forward(GIT_OID_HEXSZ + 1);

		ptr = buf;

		/* Seek forward to the end of the signature. */
		while (*buf && *buf != '\t' && *buf != '\n')
			seek_forward(1);

		if (git_signature__parse(entry->committer, &ptr, buf + 1, NULL, *buf) < 0)
			goto fail;

		if (*buf == '\t') {
			/* We got a message. Read everything till we reach LF. */
			seek_forward(1);
			ptr = buf;

			while (*buf && *buf != '\n')
				seek_forward(1);

			entry->msg = git__strndup(ptr, buf - ptr);
			GITERR_CHECK_ALLOC(entry->msg);
		} else
			entry->msg = NULL;

		while (*buf && *buf == '\n' && buf_size > 1)
			seek_forward(1);

		if (git_vector_insert(&log->entries, entry) < 0)
			goto fail;
	}

	return 0;

#undef seek_forward

fail:
	if (entry) {
		git__free(entry->committer);
		git__free(entry);
	}
	return -1;
}

void git_reflog_free(git_reflog *reflog)
{
	unsigned int i;
	git_reflog_entry *entry;

	for (i=0; i < reflog->entries.length; i++) {
		entry = git_vector_get(&reflog->entries, i);

		git_signature_free(entry->committer);

		git__free(entry->msg);
		git__free(entry);
	}

	git_vector_free(&reflog->entries);
	git__free(reflog->ref_name);
	git__free(reflog);
}

int git_reflog_read(git_reflog **reflog, git_reference *ref)
{
	int error;
	git_buf log_path = GIT_BUF_INIT;
	git_buf log_file = GIT_BUF_INIT;
	git_reflog *log = NULL;

	*reflog = NULL;

	if (reflog_init(&log, ref) < 0)
		return -1;

	error = git_buf_join_n(&log_path, '/', 3,
		ref->owner->path_repository, GIT_REFLOG_DIR, ref->name);

	if (!error)
		error = git_futils_readbuffer(&log_file, log_path.ptr);

	if (!error)
		error = reflog_parse(log, log_file.ptr, log_file.size);

	if (!error)
		*reflog = log;
	else
		git_reflog_free(log);

	git_buf_free(&log_file);
	git_buf_free(&log_path);

	return error;
}

int git_reflog_write(git_reference *ref, const git_oid *oid_old,
				const git_signature *committer, const char *msg)
{
	int error;
	char old[GIT_OID_HEXSZ+1];
	char new[GIT_OID_HEXSZ+1];
	git_buf log_path = GIT_BUF_INIT;
	git_reference *r;
	const git_oid *oid;

	if ((error = git_reference_resolve(&r, ref)) < 0)
		return error;

	oid = git_reference_oid(r);
	if (oid == NULL) {
		giterr_set(GITERR_REFERENCE,
			"Failed to write reflog. Cannot resolve reference `%s`", r->name);
		git_reference_free(r);
		return -1;
	}

	git_oid_tostr(new, GIT_OID_HEXSZ+1, oid);

	git_reference_free(r);

	error = git_buf_join_n(&log_path, '/', 3,
		ref->owner->path_repository, GIT_REFLOG_DIR, ref->name);
	if (error < 0)
		goto cleanup;

	if (git_path_exists(log_path.ptr) == false) {
		error = git_futils_mkpath2file(log_path.ptr, GIT_REFLOG_DIR_MODE);
	} else if (git_path_isfile(log_path.ptr) == false) {
		giterr_set(GITERR_REFERENCE,
			"Failed to write reflog. `%s` is directory", log_path.ptr);
		error = -1;
	} else if (oid_old == NULL) {
		giterr_set(GITERR_REFERENCE,
			"Failed to write reflog. Old OID cannot be NULL for existing reference");
		error = -1;
	}
	if (error < 0)
		goto cleanup;

	if (oid_old)
		git_oid_tostr(old, sizeof(old), oid_old);
	else
		p_snprintf(old, sizeof(old), "%0*d", GIT_OID_HEXSZ, 0);

	error = reflog_write(log_path.ptr, old, new, committer, msg);

cleanup:
	git_buf_free(&log_path);
	return error;
}

int git_reflog_rename(git_reference *ref, const char *new_name)
{
	int error;
	git_buf old_path = GIT_BUF_INIT;
	git_buf new_path = GIT_BUF_INIT;

	if (!git_buf_join_n(&old_path, '/', 3, ref->owner->path_repository,
			GIT_REFLOG_DIR, ref->name) &&
		!git_buf_join_n(&new_path, '/', 3, ref->owner->path_repository,
			GIT_REFLOG_DIR, new_name))
		error = p_rename(git_buf_cstr(&old_path), git_buf_cstr(&new_path));
	else
		error = -1;

	git_buf_free(&old_path);
	git_buf_free(&new_path);

	return error;
}

int git_reflog_delete(git_reference *ref)
{
	int error;
	git_buf path = GIT_BUF_INIT;

	error = git_buf_join_n(
		&path, '/', 3, ref->owner->path_repository, GIT_REFLOG_DIR, ref->name);

	if (!error && git_path_exists(path.ptr))
		error = p_unlink(path.ptr);

	git_buf_free(&path);

	return error;
}

unsigned int git_reflog_entrycount(git_reflog *reflog)
{
	assert(reflog);
	return reflog->entries.length;
}

const git_reflog_entry * git_reflog_entry_byindex(git_reflog *reflog, unsigned int idx)
{
	assert(reflog);
	return git_vector_get(&reflog->entries, idx);
}

const git_oid * git_reflog_entry_oidold(const git_reflog_entry *entry)
{
	assert(entry);
	return &entry->oid_old;
}

const git_oid * git_reflog_entry_oidnew(const git_reflog_entry *entry)
{
	assert(entry);
	return &entry->oid_cur;
}

git_signature * git_reflog_entry_committer(const git_reflog_entry *entry)
{
	assert(entry);
	return entry->committer;
}

char * git_reflog_entry_msg(const git_reflog_entry *entry)
{
	assert(entry);
	return entry->msg;
}
