/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "reflog.h"
#include "repository.h"
#include "filebuf.h"
#include "signature.h"

static int reflog_init(git_reflog **reflog, const git_reference *ref)
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

	log->owner = git_reference_owner(ref);
	*reflog = log;

	return 0;
}

static int serialize_reflog_entry(
	git_buf *buf,
	const git_oid *oid_old,
	const git_oid *oid_new,
	const git_signature *committer,
	const char *msg)
{
	char raw_old[GIT_OID_HEXSZ+1];
	char raw_new[GIT_OID_HEXSZ+1];

	git_oid_tostr(raw_old, GIT_OID_HEXSZ+1, oid_old);
	git_oid_tostr(raw_new, GIT_OID_HEXSZ+1, oid_new);

	git_buf_clear(buf);

	git_buf_puts(buf, raw_old);
	git_buf_putc(buf, ' ');
	git_buf_puts(buf, raw_new);

	git_signature__writebuf(buf, " ", committer);

	/* drop trailing LF */
	git_buf_rtrim(buf);

	if (msg) {
		git_buf_putc(buf, '\t');
		git_buf_puts(buf, msg);
	}

	git_buf_putc(buf, '\n');

	return git_buf_oom(buf);
}

static int reflog_entry_new(git_reflog_entry **entry)
{
	git_reflog_entry *e;

	assert(entry);

	e = git__malloc(sizeof(git_reflog_entry));
	GITERR_CHECK_ALLOC(e);

	memset(e, 0, sizeof(git_reflog_entry));

	*entry = e;

	return 0;
}

static void reflog_entry_free(git_reflog_entry *entry)
{
	git_signature_free(entry->committer);

	git__free(entry->msg);
	git__free(entry);
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
		if (reflog_entry_new(&entry) < 0)
			return -1;

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
	if (entry)
		reflog_entry_free(entry);

	return -1;
}

void git_reflog_free(git_reflog *reflog)
{
	size_t i;
	git_reflog_entry *entry;

	if (reflog == NULL)
		return;

	for (i=0; i < reflog->entries.length; i++) {
		entry = git_vector_get(&reflog->entries, i);

		reflog_entry_free(entry);
	}

	git_vector_free(&reflog->entries);
	git__free(reflog->ref_name);
	git__free(reflog);
}

static int retrieve_reflog_path(git_buf *path, const git_reference *ref)
{
	return git_buf_join_n(path, '/', 3,
		git_reference_owner(ref)->path_repository, GIT_REFLOG_DIR, ref->name);
}

static int create_new_reflog_file(const char *filepath)
{
	int fd, error;

	if ((error = git_futils_mkpath2file(filepath, GIT_REFLOG_DIR_MODE)) < 0)
		return error;

	if ((fd = p_open(filepath,
			O_WRONLY | O_CREAT | O_TRUNC,
			GIT_REFLOG_FILE_MODE)) < 0)
		return -1;

	return p_close(fd);
}

int git_reflog_read(git_reflog **reflog, const git_reference *ref)
{
	int error = -1;
	git_buf log_path = GIT_BUF_INIT;
	git_buf log_file = GIT_BUF_INIT;
	git_reflog *log = NULL;

	assert(reflog && ref);

	*reflog = NULL;

	if (reflog_init(&log, ref) < 0)
		return -1;

	if (retrieve_reflog_path(&log_path, ref) < 0)
		goto cleanup;

	error = git_futils_readbuffer(&log_file, git_buf_cstr(&log_path));
	if (error < 0 && error != GIT_ENOTFOUND)
		goto cleanup;

	if ((error == GIT_ENOTFOUND) &&
		((error = create_new_reflog_file(git_buf_cstr(&log_path))) < 0))
		goto cleanup;

	if ((error = reflog_parse(log,
		git_buf_cstr(&log_file), git_buf_len(&log_file))) < 0)
		goto cleanup;

	*reflog = log;
	goto success;

cleanup:
	git_reflog_free(log);

success:
	git_buf_free(&log_file);
	git_buf_free(&log_path);

	return error;
}

int git_reflog_write(git_reflog *reflog)
{
	int error = -1;
	unsigned int i;
	git_reflog_entry *entry;
	git_buf log_path = GIT_BUF_INIT;
	git_buf log = GIT_BUF_INIT;
	git_filebuf fbuf = GIT_FILEBUF_INIT;

	assert(reflog);

	if (git_buf_join_n(&log_path, '/', 3,
		git_repository_path(reflog->owner), GIT_REFLOG_DIR, reflog->ref_name) < 0)
		return -1;

	if (!git_path_isfile(git_buf_cstr(&log_path))) {
		giterr_set(GITERR_INVALID,
			"Log file for reference '%s' doesn't exist.", reflog->ref_name);
		goto cleanup;
	}

	if ((error = git_filebuf_open(&fbuf, git_buf_cstr(&log_path), 0)) < 0)
		goto cleanup;

	git_vector_foreach(&reflog->entries, i, entry) {
		if (serialize_reflog_entry(&log, &(entry->oid_old), &(entry->oid_cur), entry->committer, entry->msg) < 0)
			goto cleanup;

		if ((error = git_filebuf_write(&fbuf, log.ptr, log.size)) < 0)
			goto cleanup;
	}

	error = git_filebuf_commit(&fbuf, GIT_REFLOG_FILE_MODE);
	goto success;

cleanup:
	git_filebuf_cleanup(&fbuf);

success:
	git_buf_free(&log);
	git_buf_free(&log_path);
	return error;
}

int git_reflog_append(git_reflog *reflog, const git_oid *new_oid,
				const git_signature *committer, const char *msg)
{
	git_reflog_entry *entry;
	const git_reflog_entry *previous;
	const char *newline;

	assert(reflog && new_oid && committer);

	if (reflog_entry_new(&entry) < 0)
		return -1;

	if ((entry->committer = git_signature_dup(committer)) == NULL)
		goto cleanup;

	if (msg != NULL) {
		if ((entry->msg = git__strdup(msg)) == NULL)
			goto cleanup;

		newline = strchr(msg, '\n');

		if (newline) {
			if (newline[1] != '\0') {
				giterr_set(GITERR_INVALID, "Reflog message cannot contain newline");
				goto cleanup;
			}

			entry->msg[newline - msg] = '\0';
		}
	}

	previous = git_reflog_entry_byindex(reflog, 0);

	if (previous == NULL)
		git_oid_fromstr(&entry->oid_old, GIT_OID_HEX_ZERO);
	else
		git_oid_cpy(&entry->oid_old, &previous->oid_cur);

	git_oid_cpy(&entry->oid_cur, new_oid);

	if (git_vector_insert(&reflog->entries, entry) < 0)
		goto cleanup;

	return 0;

cleanup:
	reflog_entry_free(entry);
	return -1;
}

int git_reflog_rename(git_reference *ref, const char *new_name)
{
	int error = 0, fd;
	git_buf old_path = GIT_BUF_INIT;
	git_buf new_path = GIT_BUF_INIT;
	git_buf temp_path = GIT_BUF_INIT;
	git_buf normalized = GIT_BUF_INIT;

	assert(ref && new_name);

	if ((error = git_reference__normalize_name(
		&normalized, new_name, GIT_REF_FORMAT_ALLOW_ONELEVEL)) < 0)
			return error;

	if (git_buf_joinpath(&temp_path, git_reference_owner(ref)->path_repository, GIT_REFLOG_DIR) < 0)
		return -1;

	if (git_buf_joinpath(&old_path, git_buf_cstr(&temp_path), ref->name) < 0)
		return -1;

	if (git_buf_joinpath(&new_path, git_buf_cstr(&temp_path), git_buf_cstr(&normalized)) < 0)
		return -1;

	/*
	 * Move the reflog to a temporary place. This two-phase renaming is required
	 * in order to cope with funny renaming use cases when one tries to move a reference
	 * to a partially colliding namespace:
	 *  - a/b -> a/b/c
	 *  - a/b/c/d -> a/b/c
	 */
	if (git_buf_joinpath(&temp_path, git_buf_cstr(&temp_path), "temp_reflog") < 0)
		return -1;

	if ((fd = git_futils_mktmp(&temp_path, git_buf_cstr(&temp_path))) < 0) {
		error = -1;
		goto cleanup;
	}

	p_close(fd);

	if (p_rename(git_buf_cstr(&old_path), git_buf_cstr(&temp_path)) < 0) {
		giterr_set(GITERR_OS, "Failed to rename reflog for %s", new_name);
		error = -1;
		goto cleanup;
	}

	if (git_path_isdir(git_buf_cstr(&new_path)) && 
		(git_futils_rmdir_r(git_buf_cstr(&new_path), NULL, GIT_RMDIR_SKIP_NONEMPTY) < 0)) {
		error = -1;
		goto cleanup;
	}

	if (git_futils_mkpath2file(git_buf_cstr(&new_path), GIT_REFLOG_DIR_MODE) < 0) {
		error = -1;
		goto cleanup;
	}

	if (p_rename(git_buf_cstr(&temp_path), git_buf_cstr(&new_path)) < 0) {
		giterr_set(GITERR_OS, "Failed to rename reflog for %s", new_name);
		error = -1;
	}

cleanup:
	git_buf_free(&temp_path);
	git_buf_free(&old_path);
	git_buf_free(&new_path);
	git_buf_free(&normalized);

	return error;
}

int git_reflog_delete(git_reference *ref)
{
	int error;
	git_buf path = GIT_BUF_INIT;

	error = retrieve_reflog_path(&path, ref);

	if (!error && git_path_exists(path.ptr))
		error = p_unlink(path.ptr);

	git_buf_free(&path);

	return error;
}

size_t git_reflog_entrycount(git_reflog *reflog)
{
	assert(reflog);
	return reflog->entries.length;
}

GIT_INLINE(size_t) reflog_inverse_index(size_t idx, size_t total)
{
	return (total - 1) - idx;
}

const git_reflog_entry * git_reflog_entry_byindex(git_reflog *reflog, size_t idx)
{
	assert(reflog);

	if (idx >= reflog->entries.length)
		return NULL;

	return git_vector_get(
		&reflog->entries, reflog_inverse_index(idx, reflog->entries.length));
}

const git_oid * git_reflog_entry_id_old(const git_reflog_entry *entry)
{
	assert(entry);
	return &entry->oid_old;
}

const git_oid * git_reflog_entry_id_new(const git_reflog_entry *entry)
{
	assert(entry);
	return &entry->oid_cur;
}

const git_signature * git_reflog_entry_committer(const git_reflog_entry *entry)
{
	assert(entry);
	return entry->committer;
}

const char * git_reflog_entry_message(const git_reflog_entry *entry)
{
	assert(entry);
	return entry->msg;
}

int git_reflog_drop(
	git_reflog *reflog,
	size_t idx,
	int rewrite_previous_entry)
{
	size_t entrycount;
	git_reflog_entry *entry, *previous;

	assert(reflog);

	entrycount = git_reflog_entrycount(reflog);

	entry = (git_reflog_entry *)git_reflog_entry_byindex(reflog, idx);

	if (entry == NULL)
		return GIT_ENOTFOUND;

	reflog_entry_free(entry);

	if (git_vector_remove(
			&reflog->entries, reflog_inverse_index(idx, entrycount)) < 0)
		return -1;

	if (!rewrite_previous_entry)
		return 0;

	/* No need to rewrite anything when removing the most recent entry */
	if (idx == 0)
		return 0;

	/* Have the latest entry just been dropped? */
	if (entrycount == 1)
		return 0;

	entry = (git_reflog_entry *)git_reflog_entry_byindex(reflog, idx - 1);

	/* If the oldest entry has just been removed... */
	if (idx == entrycount - 1) {
		/* ...clear the oid_old member of the "new" oldest entry */
		if (git_oid_fromstr(&entry->oid_old, GIT_OID_HEX_ZERO) < 0)
			return -1;

		return 0;
	}

	previous = (git_reflog_entry *)git_reflog_entry_byindex(reflog, idx);
	git_oid_cpy(&entry->oid_old, &previous->oid_cur);

	return 0;
}
