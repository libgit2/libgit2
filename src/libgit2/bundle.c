/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "bundle.h"
#include "common.h"
#include "futils.h"
#include "hashmap_oid.h"
#include "oid.h"
#include "posix.h"
#include "str.h"
#include "vector.h"

#include "git2/bundle.h"
#include "git2/commit.h"
#include "git2/net.h"
#include "git2/object.h"
#include "git2/odb.h"
#include "git2/odb_backend.h"
#include "git2/pack.h"
#include "git2/refs.h"
#include "git2/repository.h"
#include "git2/revwalk.h"
#include "git2/strarray.h"
#include "git2/types.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/*
 * Determine the expected OID hex length for the given oid_type.
 * Defined here so it compiles without GIT_EXPERIMENTAL_SHA256.
 */
static size_t bundle_oid_hexlen(git_oid_t oid_type)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	return git_oid_hexsize(oid_type);
#else
	GIT_UNUSED(oid_type);
	return GIT_OID_SHA1_HEXSIZE;
#endif
}

/*
 * Parse a hex OID of the expected length from `str`.  `len` is the
 * total number of characters available at `str`.
 */
static int bundle_parse_oid(
	git_oid *out,
	const char *str,
	size_t len,
	git_oid_t oid_type)
{
	size_t hexsize = bundle_oid_hexlen(oid_type);

	if (len < hexsize) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: OID field too short (expected %zu, got %zu)",
			hexsize, len);
		return GIT_EINVALID;
	}

#ifdef GIT_EXPERIMENTAL_SHA256
	return git_oid_from_prefix(out, str, hexsize, oid_type);
#else
	return git_oid_fromstrn(out, str, GIT_OID_SHA1_HEXSIZE);
#endif
}

static void free_remote_head(git_remote_head *head)
{
	if (!head)
		return;
	git__free(head->name);
	git__free(head->symref_target);
	git__free(head);
}

/* -------------------------------------------------------------------------
 * Header parsing
 * ---------------------------------------------------------------------- */

/*
 * Parse one capability line of the form "@key=value\n".
 * Currently we only understand "object-format".
 */
static int parse_capability_line(
	git_bundle *bundle,
	const char *line,
	size_t len)
{
	/* strip leading '@' */
	GIT_ASSERT(len > 0 && line[0] == '@');
	line++;
	len--;

	if (git__prefixncmp(line, len, "object-format=") == 0) {
		const char *val = line + strlen("object-format=");
		size_t vlen = len - strlen("object-format=");

		if (bundle->capabilities & GIT_BUNDLE_CAP_OBJECT_FORMAT) {
			git_error_set(GIT_ERROR_INVALID,
				"bundle: duplicate 'object-format' capability");
			return GIT_EINVALID;
		}

		bundle->capabilities |= GIT_BUNDLE_CAP_OBJECT_FORMAT;

#ifdef GIT_EXPERIMENTAL_SHA256
		if (git__prefixncmp(val, vlen, "sha256") == 0 &&
		    (vlen == strlen("sha256"))) {
			bundle->oid_type = GIT_OID_SHA256;
		} else if (git__prefixncmp(val, vlen, "sha1") == 0 &&
		           (vlen == strlen("sha1"))) {
			bundle->oid_type = GIT_OID_SHA1;
		} else {
#else
		if (git__prefixncmp(val, vlen, "sha1") == 0 &&
		    (vlen == strlen("sha1"))) {
			bundle->oid_type = GIT_OID_SHA1;
		} else {
#endif
			git_error_set(GIT_ERROR_INVALID,
				"bundle: unsupported object-format capability");
			return GIT_ENOTSUPPORTED;
		}
	} else {
		/*
		 * Unknown capabilities in v3 must be rejected so that we do
		 * not silently produce incorrect results.
		 */
		git_error_set(GIT_ERROR_INVALID,
			"bundle: unsupported capability");
		return GIT_ENOTSUPPORTED;
	}

	return 0;
}

/*
 * Parse a prerequisite line of the form "-<oid> <comment>\n".
 */
static int parse_prerequisite_line(
	git_bundle *bundle,
	const char *line,
	size_t len)
{
	git_oid oid, *oid_copy;
	size_t hexsize = bundle_oid_hexlen(bundle->oid_type);
	int error;

	/* strip leading '-' */
	GIT_ASSERT(len > 0 && line[0] == '-');
	line++;
	len--;

	if ((error = bundle_parse_oid(&oid, line, len, bundle->oid_type)) < 0)
		return error;

	/* must be followed by a space */
	if (len <= hexsize || line[hexsize] != ' ') {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: malformed prerequisite line");
		return GIT_EINVALID;
	}

	oid_copy = git__malloc(sizeof(git_oid));
	GIT_ERROR_CHECK_ALLOC(oid_copy);
	git_oid_cpy(oid_copy, &oid);

	if ((error = git_vector_insert(&bundle->prerequisites, oid_copy)) < 0) {
		git__free(oid_copy);
		return error;
	}

	return 0;
}

/*
 * Parse a reference line of the form "<oid> <refname>\n".
 */
static int parse_ref_line(
	git_bundle *bundle,
	const char *line,
	size_t len)
{
	git_oid oid;
	git_remote_head *head;
	size_t hexsize = bundle_oid_hexlen(bundle->oid_type);
	int error;

	if ((error = bundle_parse_oid(&oid, line, len, bundle->oid_type)) < 0)
		return error;

	/* must have a space followed by at least one refname character */
	if (len <= hexsize + 1 || line[hexsize] != ' ') {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: malformed reference line");
		return GIT_EINVALID;
	}

	head = git__calloc(1, sizeof(git_remote_head));
	GIT_ERROR_CHECK_ALLOC(head);

	git_oid_cpy(&head->oid, &oid);

	/* deep-copy the refname; never store a pointer into the parse buffer */
	head->name = git__strndup(line + hexsize + 1, len - hexsize - 1);
	if (!head->name) {
		git__free(head);
		return -1;
	}

	if ((error = git_vector_insert(&bundle->refs, head)) < 0) {
		free_remote_head(head);
		return error;
	}

	return 0;
}

/*
 * Parse the header portion of a bundle from a memory buffer.
 * `buf` must contain at least the header up to and including the blank
 * separator line.  `file_size` is used only to validate that the stored
 * pack_start_offset is in range; pass 0 to skip that check.
 *
 * On success, bundle->pack_start_offset and bundle->header_len are set.
 */
static int bundle_parse_header(
	git_bundle *bundle,
	const char *buf,
	size_t buflen,
	uint64_t file_size)
{
	const char *p = buf;
	const char *end = buf + buflen;
	const char *sep;
	int error = 0;
	bool have_signature = false;
	bool in_prerequisites = false; /* once refs start, prereqs must stop */

	/* ----------------------------------------------------------------
	 * Find the blank-line separator "\n\n".
	 * The separator is the empty line between the last header line and
	 * the packfile.  In the byte stream the pattern is: the newline
	 * that ends the last header line immediately followed by another
	 * newline (the empty separator line).
	 * ---------------------------------------------------------------- */
	sep = p;
	while (sep < end - 1) {
		if (sep[0] == '\n' && sep[1] == '\n')
			break;
		sep++;
	}

	if (sep >= end - 1) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: header separator not found "
			"(file may be truncated or not a bundle)");
		return GIT_EINVALID;
	}

	/*
	 * pack_start_offset = position of byte after the second '\n'.
	 */
	bundle->pack_start_offset = (size_t)(sep - buf) + 2;
	bundle->header_len        = bundle->pack_start_offset;

	if (file_size > 0 && bundle->pack_start_offset > file_size) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: pack offset exceeds file size");
		return GIT_EINVALID;
	}

	/* ----------------------------------------------------------------
	 * Parse line by line up to (but not including) the separator.
	 * ---------------------------------------------------------------- */
	while (p <= sep) {
		const char *nl = memchr(p, '\n', (size_t)(sep - p) + 1);
		size_t linelen;

		if (!nl)
			break; /* reached separator region */

		linelen = (size_t)(nl - p);

		/* Reject CRLF */
		if (linelen > 0 && p[linelen - 1] == '\r') {
			git_error_set(GIT_ERROR_INVALID,
				"bundle: CRLF line endings are not supported");
			return GIT_EINVALID;
		}

		if (!have_signature) {
			/* First line must be the bundle signature */
			if (linelen == strlen(GIT_BUNDLE_SIGNATURE_V2) - 1 &&
			    memcmp(p, GIT_BUNDLE_SIGNATURE_V2,
			           strlen(GIT_BUNDLE_SIGNATURE_V2) - 1) == 0) {
				bundle->version  = 2;
				bundle->oid_type = GIT_OID_SHA1;
			} else if (linelen == strlen(GIT_BUNDLE_SIGNATURE_V3) - 1 &&
			           memcmp(p, GIT_BUNDLE_SIGNATURE_V3,
			                  strlen(GIT_BUNDLE_SIGNATURE_V3) - 1) == 0) {
				bundle->version  = 3;
				bundle->oid_type = GIT_OID_SHA1; /* default; may be overridden */
			} else {
				git_error_set(GIT_ERROR_INVALID,
					"bundle: invalid signature (not a git bundle)");
				return GIT_EINVALID;
			}
			have_signature = true;

		} else if (linelen == 0) {
			/* blank line: this should only be the separator */
			break;

		} else if (bundle->version == 3 && p[0] == '@') {
			/* v3 capability line */
			if (in_prerequisites) {
				git_error_set(GIT_ERROR_INVALID,
					"bundle: capability after prerequisites");
				return GIT_EINVALID;
			}
			if (bundle->refs.length > 0) {
				git_error_set(GIT_ERROR_INVALID,
					"bundle: capability after references");
				return GIT_EINVALID;
			}
			if ((error = parse_capability_line(bundle, p, linelen)) < 0)
				return error;

		} else if (p[0] == '-') {
			/* prerequisite line */
			if (bundle->refs.length > 0) {
				git_error_set(GIT_ERROR_INVALID,
					"bundle: prerequisite after reference");
				return GIT_EINVALID;
			}
			in_prerequisites = true;
			if ((error = parse_prerequisite_line(bundle, p, linelen)) < 0)
				return error;

		} else {
			/* reference line */
			if ((error = parse_ref_line(bundle, p, linelen)) < 0)
				return error;

		}

		p = nl + 1;
	}

	if (!have_signature) {
		git_error_set(GIT_ERROR_INVALID, "bundle: missing signature");
		return GIT_EINVALID;
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * Public API – reading
 * ---------------------------------------------------------------------- */

int git_bundle_is_valid(const char *path)
{
	git_file fd;
	char buf[64];
	ssize_t nread;
	int valid = 0;

	GIT_ASSERT_ARG(path);

	fd = p_open(path, O_RDONLY, 0);
	if (fd < 0) {
		if (errno == ENOENT || errno == ENOTDIR)
			return 0;
		git_error_set(GIT_ERROR_OS, "bundle: cannot open '%s'", path);
		return -1;
	}

	nread = p_read(fd, buf, sizeof(buf) - 1);
	p_close(fd);

	if (nread < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: failed to read '%s'", path);
		return -1;
	}
	buf[nread] = '\0';

	if (strncmp(buf, GIT_BUNDLE_SIGNATURE_V2,
	            strlen(GIT_BUNDLE_SIGNATURE_V2)) == 0 ||
	    strncmp(buf, GIT_BUNDLE_SIGNATURE_V3,
	            strlen(GIT_BUNDLE_SIGNATURE_V3)) == 0)
		valid = 1;

	return valid;
}

int git_bundle_open(git_bundle **out, const char *path)
{
	git_bundle *bundle = NULL;
	git_str buf = GIT_STR_INIT;
	git_file fd;
	struct stat st;
	uint64_t file_size = 0;
	int error;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(path);

	bundle = git__calloc(1, sizeof(git_bundle));
	GIT_ERROR_CHECK_ALLOC(bundle);

	if ((error = git_vector_init(&bundle->prerequisites, 4, NULL)) < 0 ||
	    (error = git_vector_init(&bundle->refs, 64, NULL)) < 0)
		goto done;

	fd = p_open(path, O_RDONLY, 0);
	if (fd < 0) {
		if (errno == ENOENT || errno == ENOTDIR) {
			git_error_set(GIT_ERROR_OS,
				"bundle: file not found: '%s'", path);
			error = GIT_ENOTFOUND;
		} else {
			git_error_set(GIT_ERROR_OS,
				"bundle: cannot open '%s'", path);
			error = -1;
		}
		goto done;
	}

	if (p_fstat(fd, &st) == 0)
		file_size = (uint64_t)st.st_size;

	/*
	 * Read up to GIT_BUNDLE_MAX_HEADER_SIZE bytes (or the whole file
	 * if it is smaller).  We need to find the header separator "\n\n"
	 * which must appear before the packfile starts.
	 */
	{
		size_t to_read = GIT_BUNDLE_MAX_HEADER_SIZE;
		if (file_size > 0 && (uint64_t)to_read > file_size)
			to_read = (size_t)file_size;
		error = git_futils_readbuffer_fd(&buf, fd, to_read);
	}
	p_close(fd);

	if (error < 0)
		goto done;

	error = bundle_parse_header(bundle, buf.ptr, buf.size, file_size);
	if (error < 0)
		goto done;

	if ((error = git_str_sets(&bundle->path, path)) < 0)
		goto done;

	*out = bundle;
	bundle = NULL; /* caller owns it */

done:
	git_str_dispose(&buf);
	if (bundle)
		git_bundle_free(bundle);
	return error;
}

int git_bundle_refs(
	const git_remote_head ***out,
	size_t *count,
	git_bundle *bundle)
{
	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(count);
	GIT_ASSERT_ARG(bundle);

	*out   = (const git_remote_head **)bundle->refs.contents;
	*count = bundle->refs.length;
	return 0;
}

int git_bundle_prerequisites(git_oidarray *out, git_bundle *bundle)
{
	git_oid *oid;
	size_t i;

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(bundle);

	memset(out, 0, sizeof(*out));

	if (bundle->prerequisites.length == 0)
		return 0;

	out->ids = git__calloc(
		bundle->prerequisites.length, sizeof(git_oid));
	GIT_ERROR_CHECK_ALLOC(out->ids);

	git_vector_foreach(&bundle->prerequisites, i, oid)
		git_oid_cpy(&out->ids[i], oid);

	out->count = bundle->prerequisites.length;
	return 0;
}

int git_bundle_verify(git_repository *repo, git_bundle *bundle)
{
	git_odb *odb = NULL;
	git_odb_object *obj = NULL;
	git_oid *prereq;
	size_t i;
	int error = 0;

	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(bundle);

	/* Bundle with no prerequisites is always satisfied */
	if (bundle->prerequisites.length == 0)
		return 0;

	if ((error = git_repository_odb(&odb, repo)) < 0)
		return error;

	git_vector_foreach(&bundle->prerequisites, i, prereq) {
		git_object_t type;
		size_t olen;

		/* Check existence and type in one ODB call */
		error = git_odb_read_header(&olen, &type, odb, prereq);
		if (error == GIT_ENOTFOUND) {
			git_error_set(GIT_ERROR_REFERENCE,
				"bundle: prerequisite commit %s is not present "
				"in the repository",
				git_oid_tostr_s(prereq));
			error = GIT_ENOTFOUND;
			goto done;
		} else if (error < 0) {
			goto done;
		}

		if (type != GIT_OBJECT_COMMIT) {
			git_error_set(GIT_ERROR_INVALID,
				"bundle: prerequisite %s is not a commit "
				"(type: %s)",
				git_oid_tostr_s(prereq),
				git_object_type2string(type));
			error = GIT_EINVALID;
			goto done;
		}
	}

done:
	git_odb_object_free(obj);
	git_odb_free(odb);
	return error;
}

void git_bundle_free(git_bundle *bundle)
{
	git_oid *oid;
	git_remote_head *head;
	size_t i;

	if (!bundle)
		return;

	git_vector_foreach(&bundle->prerequisites, i, oid)
		git__free(oid);
	git_vector_dispose(&bundle->prerequisites);

	git_vector_foreach(&bundle->refs, i, head)
		free_remote_head(head);
	git_vector_dispose(&bundle->refs);

	git_str_dispose(&bundle->path);
	git__free(bundle);
}

int git_bundle_create_options_init(
	git_bundle_create_options *opts,
	unsigned int version)
{
	GIT_INIT_STRUCTURE_FROM_TEMPLATE(
		opts, version, git_bundle_create_options,
		GIT_BUNDLE_CREATE_OPTIONS_INIT);
	return 0;
}

/* -------------------------------------------------------------------------
 * Bundle creation
 * ---------------------------------------------------------------------- */

/*
 * Hashmap used during bundle creation to track which commit OIDs are
 * included in the revwalk, so we can detect prerequisites (parent commits
 * that are NOT in the walk).
 */
GIT_HASHMAP_OID_SETUP(bundle_oidset, int);

typedef struct {
	git_file fd;
	int error;
} bundle_write_data;

static int bundle_write_pack_cb(void *buf, size_t len, void *payload)
{
	bundle_write_data *wd = payload;

	if (p_write(wd->fd, buf, len) < 0) {
		git_error_set(GIT_ERROR_OS, "bundle: failed to write packfile");
		wd->error = -1;
		return -1;
	}
	return 0;
}

/*
 * Write the bundle header to `fd`.
 *
 * Format:
 *   # v2 git bundle\n          (or v3)
 *   [@object-format=sha256\n]  (v3 + sha256 only)
 *   -<oid> <comment>\n ...     (prerequisites)
 *   <oid> <refname>\n ...      (references)
 *   \n                         (blank separator)
 */
static int bundle_write_header(
	git_file fd,
	unsigned int bundle_version,
	git_oid_t oid_type,
	const git_vector *prerequisites,  /* git_oid* */
	const git_vector *bundle_refs)    /* git_remote_head* */
{
	git_str hdr = GIT_STR_INIT;
	git_oid *prereq;
	git_remote_head *head;
	size_t i;
	int error = 0;

	/* signature */
	if (bundle_version == 3)
		git_str_puts(&hdr, GIT_BUNDLE_SIGNATURE_V3);
	else
		git_str_puts(&hdr, GIT_BUNDLE_SIGNATURE_V2);

	/* v3 capabilities */
#ifdef GIT_EXPERIMENTAL_SHA256
	if (bundle_version == 3 && oid_type == GIT_OID_SHA256)
		git_str_puts(&hdr, "@object-format=sha256\n");
#else
	GIT_UNUSED(oid_type);
#endif

	/* prerequisites */
	git_vector_foreach(prerequisites, i, prereq) {
		char hex[GIT_OID_MAX_HEXSIZE + 1];
		git_oid_tostr(hex, sizeof(hex), prereq);
		git_str_printf(&hdr, "-%s prerequisite\n", hex);
	}

	/* references */
	git_vector_foreach(bundle_refs, i, head) {
		char hex[GIT_OID_MAX_HEXSIZE + 1];
		git_oid_tostr(hex, sizeof(hex), &head->oid);
		git_str_printf(&hdr, "%s %s\n", hex, head->name);
	}

	/* blank separator */
	git_str_putc(&hdr, '\n');

	if (git_str_oom(&hdr)) {
		error = -1;
		goto done;
	}

	if (p_write(fd, hdr.ptr, hdr.size) < 0) {
		git_error_set(GIT_ERROR_OS, "bundle: failed to write header");
		error = -1;
	}

done:
	git_str_dispose(&hdr);
	return error;
}

/*
 * Resolve `refname` to a remote_head entry for use in the bundle header.
 * Peeled tags are NOT included (bundle refs point to the tag object itself).
 */
static int bundle_resolve_ref(
	git_remote_head **out,
	git_repository *repo,
	const char *refname)
{
	git_reference *ref = NULL, *resolved = NULL;
	git_remote_head *head = NULL;
	int error;

	if ((error = git_reference_lookup(&ref, repo, refname)) < 0)
		goto done;

	if ((error = git_reference_resolve(&resolved, ref)) < 0)
		goto done;

	head = git__calloc(1, sizeof(git_remote_head));
	GIT_ERROR_CHECK_ALLOC(head);

	git_oid_cpy(&head->oid, git_reference_target(resolved));

	head->name = git__strdup(refname);
	if (!head->name) {
		git__free(head);
		head = NULL;
		error = -1;
		goto done;
	}

	*out = head;

done:
	git_reference_free(ref);
	git_reference_free(resolved);
	return error;
}

int git_bundle_create(
	const char *path,
	git_repository *repo,
	git_revwalk *walk,
	const git_strarray *refs,
	const git_bundle_create_options *opts)
{
	git_bundle_create_options default_opts = GIT_BUNDLE_CREATE_OPTIONS_INIT;
	git_packbuilder *pb = NULL;
	git_file fd = -1;
	git_vector walk_commits  = GIT_VECTOR_INIT; /* git_oid* of visited commits */
	git_vector prereqs       = GIT_VECTOR_INIT; /* git_oid* prerequisites */
	git_vector bundle_refs   = GIT_VECTOR_INIT; /* git_remote_head* */
	bundle_oidset included   = GIT_HASHMAP_INIT;
	bundle_write_data wd     = {0};
	git_oid walk_oid;
	size_t i;
	int error = 0;

	GIT_ASSERT_ARG(path);
	GIT_ASSERT_ARG(repo);
	GIT_ASSERT_ARG(walk);

	if (!opts)
		opts = &default_opts;

	if (opts->bundle_version != 2 && opts->bundle_version != 3) {
		git_error_set(GIT_ERROR_INVALID,
			"bundle: unsupported bundle_version %u",
			opts->bundle_version);
		return GIT_EINVALID;
	}

	if ((error = git_vector_init(&walk_commits, 256, NULL)) < 0 ||
	    (error = git_vector_init(&prereqs, 8, NULL)) < 0 ||
	    (error = git_vector_init(&bundle_refs, 8, NULL)) < 0)
		goto cleanup;

	/* ------------------------------------------------------------------
	 * Step 1: Iterate the revwalk, collecting included commits.
	 * Insert each into the packbuilder and the included-OID set.
	 * ----------------------------------------------------------------- */
	if ((error = git_packbuilder_new(&pb, repo)) < 0)
		goto cleanup;

	while ((error = git_revwalk_next(&walk_oid, walk)) == 0) {
		git_oid *oid_copy;
		int put_err;

		oid_copy = git__malloc(sizeof(git_oid));
		GIT_ERROR_CHECK_ALLOC(oid_copy);
		git_oid_cpy(oid_copy, &walk_oid);

		if ((error = git_vector_insert(&walk_commits, oid_copy)) < 0) {
			git__free(oid_copy);
			goto cleanup;
		}

		put_err = bundle_oidset_put(&included, oid_copy, 1);
		if (put_err < 0) {
			error = put_err;
			goto cleanup;
		}

		if ((error = git_packbuilder_insert_commit(pb, oid_copy)) < 0)
			goto cleanup;
	}

	if (error != GIT_ITEROVER)
		goto cleanup;
	error = 0;

	/* ------------------------------------------------------------------
	 * Step 2: Find prerequisites.  For each included commit, inspect its
	 * parents.  Any parent NOT in the included set is a prerequisite.
	 * ----------------------------------------------------------------- */
	for (i = 0; i < walk_commits.length; i++) {
		git_oid *coid = (git_oid *)walk_commits.contents[i];
		git_commit *commit = NULL;
		unsigned int p;

		if ((error = git_commit_lookup(&commit, repo, coid)) < 0)
			goto cleanup;

		for (p = 0; p < git_commit_parentcount(commit); p++) {
			const git_oid *parent_id = git_commit_parent_id(commit, p);

			if (!bundle_oidset_contains(&included, parent_id)) {
				/* parent not in walk → prerequisite */
				git_oid *pcopy;
				size_t j;
				bool already = false;

				/* deduplicate */
				for (j = 0; j < prereqs.length; j++) {
					if (git_oid_equal(
					        (git_oid *)prereqs.contents[j],
					        parent_id)) {
						already = true;
						break;
					}
				}

				if (!already) {
					pcopy = git__malloc(sizeof(git_oid));
					if (!pcopy) {
						git_commit_free(commit);
						error = -1;
						goto cleanup;
					}
					git_oid_cpy(pcopy, parent_id);
					if ((error = git_vector_insert(
					         &prereqs, pcopy)) < 0) {
						git__free(pcopy);
						git_commit_free(commit);
						goto cleanup;
					}
				}
			}
		}

		git_commit_free(commit);
	}

	/* ------------------------------------------------------------------
	 * Step 3: Add any explicitly requested non-commit ref objects to the
	 * packbuilder (tags, blobs, trees pointed to directly by refs).
	 * Also build the bundle_refs vector.
	 * ----------------------------------------------------------------- */
	if (refs && refs->count > 0) {
		for (i = 0; i < refs->count; i++) {
			git_remote_head *rhead = NULL;

			if ((error = bundle_resolve_ref(
			         &rhead, repo, refs->strings[i])) < 0)
				goto cleanup;

			if ((error = git_vector_insert(
			         &bundle_refs, rhead)) < 0) {
				free_remote_head(rhead);
				goto cleanup;
			}

			/* Ensure the ref target is in the pack (covers tags etc.) */
			if ((error = git_packbuilder_insert_recur(
			         pb, &rhead->oid, rhead->name)) < 0)
				goto cleanup;
		}
	} else {
		/*
		 * No explicit refs: include all local branches and tags that
		 * resolve to an OID reachable from the walked commits.
		 */
		git_strarray all_refs = {0};
		size_t r;

		if ((error = git_reference_list(&all_refs, repo)) < 0)
			goto cleanup;

		for (r = 0; r < all_refs.count; r++) {
			const char *rname = all_refs.strings[r];
			git_remote_head *rhead = NULL;

			if (strncmp(rname, "refs/heads/", 11) != 0 &&
			    strncmp(rname, "refs/tags/", 10) != 0)
				continue;

			if (bundle_resolve_ref(&rhead, repo, rname) < 0) {
				git_error_clear();
				continue;
			}

			if ((error = git_vector_insert(
			         &bundle_refs, rhead)) < 0) {
				free_remote_head(rhead);
				git_strarray_dispose(&all_refs);
				goto cleanup;
			}

			if ((error = git_packbuilder_insert_recur(
			         pb, &rhead->oid, rhead->name)) < 0) {
				git_strarray_dispose(&all_refs);
				goto cleanup;
			}
		}

		git_strarray_dispose(&all_refs);
	}

	/* ------------------------------------------------------------------
	 * Step 4: Write the bundle file.
	 * ----------------------------------------------------------------- */
	fd = p_open(path, O_WRONLY | O_CREAT | O_TRUNC,
	            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0) {
		git_error_set(GIT_ERROR_OS,
			"bundle: cannot create '%s'", path);
		error = -1;
		goto cleanup;
	}

	{
		git_oid_t oid_type = GIT_OID_SHA1;
#ifdef GIT_EXPERIMENTAL_SHA256
		if (opts->bundle_version == 3)
			oid_type = opts->oid_type;
#endif
		if ((error = bundle_write_header(
		         fd,
		         opts->bundle_version,
		         oid_type,
		         &prereqs,
		         &bundle_refs)) < 0)
			goto cleanup;
	}

	wd.fd = fd;
	git_packbuilder_set_threads(pb, 0);

	if ((error = git_packbuilder_foreach(
	         pb, bundle_write_pack_cb, &wd)) < 0)
		goto cleanup;

	if (wd.error < 0) {
		error = wd.error;
		goto cleanup;
	}

cleanup:
	{
		git_oid *oid;
		git_remote_head *head;
		size_t j;

		git_vector_foreach(&walk_commits, j, oid)
			git__free(oid);
		git_vector_dispose(&walk_commits);

		git_vector_foreach(&prereqs, j, oid)
			git__free(oid);
		git_vector_dispose(&prereqs);

		git_vector_foreach(&bundle_refs, j, head)
			free_remote_head(head);
		git_vector_dispose(&bundle_refs);
	}

	bundle_oidset_dispose(&included);

	if (pb)
		git_packbuilder_free(pb);

	if (fd >= 0)
		p_close(fd);

	/* Remove partially-written file on error */
	if (error < 0)
		p_unlink(path);

	return error;
}
