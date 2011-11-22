/*
 * Copyright (C) 2009-2011 the libgit2 contributors
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

	log = git__malloc(sizeof(git_reflog));
	if (log == NULL)
		return GIT_ENOMEM;

	memset(log, 0x0, sizeof(git_reflog));

	log->ref_name = git__strdup(ref->name);

	if (git_vector_init(&log->entries, 0, NULL) < 0) {
		git__free(log->ref_name);
		git__free(log);
		return GIT_ENOMEM;
	}

	*reflog = log;

	return GIT_SUCCESS;
}

static int reflog_write(git_path *log_path, const char *oid_old,
			const char *oid_new, const git_signature *committer,
			const char *msg)
{
	int error;
	git_buf log = GIT_BUF_INIT;
	git_filebuf fbuf = GIT_FILEBUF_INIT;

	assert(log_path && log_path->data && oid_old && oid_new && committer);

	git_buf_puts(&log, oid_old);
	git_buf_putc(&log, ' ');

	git_buf_puts(&log, oid_new);

	git_signature__writebuf(&log, " ", committer);
	log.size--; /* drop LF */

	if (msg) {
		if (strchr(msg, '\n')) {
			git_buf_free(&log);
			git__path_free(log_path);
			return git__throw(GIT_ERROR, "Reflog message cannot contain newline");
		}

		git_buf_putc(&log, '\t');
		git_buf_puts(&log, msg);
	}

	git_buf_putc(&log, '\n');

	if ((error = git_filebuf_open(&fbuf, log_path->data, GIT_FILEBUF_APPEND)) < GIT_SUCCESS) {
		git_buf_free(&log);
		git__throw(error, "Failed to write reflog. Cannot open reflog `%s`", log_path->data);
		git__path_free(log_path);
		return error;
	}

	git_filebuf_write(&fbuf, log.ptr, log.size);
	error = git_filebuf_commit(&fbuf, GIT_REFLOG_FILE_MODE);

	git_buf_free(&log);
	git__path_free(log_path);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to write reflog");
}

static int reflog_parse(git_reflog *log, const char *buf, size_t buf_size)
{
	int error = GIT_SUCCESS;
	const char *ptr;
	git_reflog_entry *entry;

#define seek_forward(_increase) { \
	if (_increase >= buf_size) { \
		if (entry->committer) \
			git__free(entry->committer); \
		git__free(entry); \
		return git__throw(GIT_ERROR, "Failed to seek forward. Buffer size exceeded"); \
	} \
	buf += _increase; \
	buf_size -= _increase; \
}

	while (buf_size > GIT_REFLOG_SIZE_MIN) {
		entry = git__malloc(sizeof(git_reflog_entry));
		if (entry == NULL)
			return GIT_ENOMEM;
		entry->committer = NULL;

		if (git_oid_fromstrn(&entry->oid_old, buf, GIT_OID_HEXSZ) < GIT_SUCCESS) {
			git__free(entry);
			return GIT_ERROR;
		}
		seek_forward(GIT_OID_HEXSZ + 1);

		if (git_oid_fromstrn(&entry->oid_cur, buf, GIT_OID_HEXSZ) < GIT_SUCCESS) {
			git__free(entry);
			return GIT_ERROR;
		}
		seek_forward(GIT_OID_HEXSZ + 1);

		ptr = buf;

		/* Seek forward to the end of the signature. */
		while (*buf && *buf != '\t' && *buf != '\n')
			seek_forward(1);

		entry->committer = git__malloc(sizeof(git_signature));
		if (entry->committer == NULL) {
			git__free(entry);
			return GIT_ENOMEM;
		}

		if ((error = git_signature__parse(entry->committer, &ptr, buf + 1, NULL, *buf)) < GIT_SUCCESS) {
			git__free(entry->committer);
			git__free(entry);
			return git__rethrow(error, "Failed to parse reflog. Could not parse signature");
		}

		if (*buf == '\t') {
			/* We got a message. Read everything till we reach LF. */
			seek_forward(1);
			ptr = buf;

			while (*buf && *buf != '\n')
				seek_forward(1);

			entry->msg = git__strndup(ptr, buf - ptr);
		} else
			entry->msg = NULL;

		while (*buf && *buf == '\n' && buf_size > 1)
			seek_forward(1);

		if ((error = git_vector_insert(&log->entries, entry)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to parse reflog. Could not add new entry");
	}

#undef seek_forward

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to parse reflog");
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
	git_path log_path = GIT_PATH_INIT;
	git_fbuffer log_file = GIT_FBUFFER_INIT;
	git_reflog *log = NULL;

	*reflog = NULL;

	if ((error = reflog_init(&log, ref)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read reflog. Cannot init reflog");

	error = git_path_join_n(&log_path, 3, ref->owner->path_repository, GIT_REFLOG_DIR, ref->name);
	if (error < GIT_SUCCESS)
		goto cleanup;

	if ((error = git_futils_readbuffer(&log_file, log_path.data)) < GIT_SUCCESS) {
		git__rethrow(error, "Failed to read reflog. Cannot read file `%s`", log_path.data);
		goto cleanup;
	}

	if ((error = reflog_parse(log, log_file.data, log_file.len)) < GIT_SUCCESS)
		git__rethrow(error, "Failed to read reflog");

	if (error == GIT_SUCCESS)
		*reflog = log;

cleanup:
	if (error != GIT_SUCCESS && log != NULL)
		git_reflog_free(log);
	git_futils_freebuffer(&log_file);
	git__path_free(&log_path);

	return error;
}

int git_reflog_write(git_reference *ref, const git_oid *oid_old,
				const git_signature *committer, const char *msg)
{
	int error;
	char old[GIT_OID_HEXSZ+1];
	char new[GIT_OID_HEXSZ+1];
	git_path log_path = GIT_PATH_INIT;
	git_reference *r;
	const git_oid *oid;

	if ((error = git_reference_resolve(&r, ref)) < GIT_SUCCESS)
		return git__rethrow(error,
			"Failed to write reflog. Cannot resolve reference `%s`", ref->name);

	oid = git_reference_oid(r);
	if (oid == NULL) {
		error = git__throw(GIT_ERROR,
			"Failed to write reflog. Cannot resolve reference `%s`", r->name);
		git_reference_free(r);
		return error;
	}

	git_reference_free(r);

	git_oid_to_string(new, GIT_OID_HEXSZ+1, oid);

	error = git_path_join_n(&log_path, 3,
		ref->owner->path_repository, GIT_REFLOG_DIR, ref->name);
	if (error < GIT_SUCCESS)
		return git__rethrow(error,
			"Failed to write reflog. Cannot allocate reflog directory");

	if (git_futils_exists(log_path.data)) {
		error = git_futils_mkpath2file(log_path.data, GIT_REFLOG_DIR_MODE);
		if (error < GIT_SUCCESS) {
			git__rethrow(error,
				"Failed to write reflog. Cannot create reflog directory");
			goto fail;
		}

	} else if (git_futils_isfile(log_path.data)) {
		error = git__throw(GIT_ERROR,
			"Failed to write reflog. `%s` is directory", log_path.data);
		goto fail;
	} else if (oid_old == NULL) {
		error = git__throw(GIT_ERROR,
			"Failed to write reflog. Old OID cannot be NULL for existing reference");
		goto fail;
	}

	if (oid_old)
		git_oid_to_string(old, GIT_OID_HEXSZ+1, oid_old);
	else
		p_snprintf(old, GIT_OID_HEXSZ+1, "%0*d", GIT_OID_HEXSZ, 0);

	return reflog_write(&log_path, old, new, committer, msg);

fail:
	git__path_free(&log_path);
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
