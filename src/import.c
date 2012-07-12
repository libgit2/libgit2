#include "git2/import.h"
#include "git2/blob.h"
#include "common.h"
#include "khash.h"

enum {
	GIT_IMPORTER_STATE_CLEAR,
	GIT_IMPORTER_STATE_BLOB,
};

__KHASH_TYPE(mark, size_t, git_oid);
typedef khash_t(mark) git_importer_mark_hashtable;

GIT_INLINE(khint_t) hash_size_t(size_t mark)
{
	khint_t h = 0;
	while (mark) {
		h = (h << 5) - h + (mark & 0x00ff);
		mark = mark >> 8;
	}
	return h;
}

GIT_INLINE(int) hash_size_t_equal(size_t a, size_t b)
{
	return (a == b);
}

__KHASH_IMPL(mark, static kh_inline, size_t, git_oid, 1,
	hash_size_t, hash_size_t_equal);

struct git_importer {
	git_repository *owner;
	int state;
	size_t mark;
	git_importer_mark_hashtable *markmap;
};

static int importer_error(git_importer *importer, const char *msg)
{
	giterr_set(GITERR_INVALID, "Import error - %s (%d)", msg, importer->state);
	return -1;
}

int git_importer_create(
	git_importer **importer_p,
	git_repository *repo)
{
	git_importer *importer;

	assert(importer_p && repo);

	importer = git__calloc(1, sizeof(git_importer));
	GITERR_CHECK_ALLOC(importer);

	importer->owner = repo;

	importer->markmap = kh_init(mark);

	*importer_p = importer;

	return 0;
}

int git_importer_free(git_importer *importer)
{
	kh_destroy(mark, importer->markmap);
	git__free(importer);

	return 0;
}

int git_importer_blob(git_importer *importer)
{
	assert(importer);

	if (importer->state != GIT_IMPORTER_STATE_CLEAR)
		return importer_error(importer, "invalid state for importing blob");

	importer->state = GIT_IMPORTER_STATE_BLOB;

	return 0;
}

int git_importer_mark(git_importer *importer, size_t mark)
{
	assert(importer && mark);

	if (importer->state != GIT_IMPORTER_STATE_BLOB)
		return importer_error(importer, "invalid state for setting mark");

	if (importer->mark != 0)
		return importer_error(importer, "attempt to set mark with prior mark active");

	importer->mark = mark;

	return 0;
}

int git_importer_data(git_importer *importer, const void *buffer, size_t len)
{
	int error;
	git_oid oid;

	assert(importer);

	if (importer->state != GIT_IMPORTER_STATE_BLOB)
		return importer_error(importer, "cannot accept data in current state");

	error = git_blob_create_frombuffer(&oid, importer->owner, buffer, len);
	if (error)
		return error;

	if (importer->mark) {
		khiter_t pos = kh_put(mark, importer->markmap, importer->mark, &error);
		if (error >= 0) {
			kh_val(importer->markmap, pos) = oid;
			error = 0;
		}
		importer->mark = 0;
	}

	importer->state = GIT_IMPORTER_STATE_CLEAR;

	return error;
}

int git_importer_cat_blob_from_oid(
	git_importer *importer,
	const git_oid *oid_p,
	git_importer_cat_blob_callback cb,
	void *payload)
{
	git_blob *blob_p;
	int error = git_blob_lookup(&blob_p, importer->owner, oid_p);
	if (error)
		return error;

	error = cb(payload, oid_p, git_blob_rawcontent(blob_p), git_blob_rawsize(blob_p));

	git_blob_free(blob_p);

	return error;
}

int git_importer_cat_blob_from_mark(
	git_importer *importer,
	size_t mark,
	git_importer_cat_blob_callback cb,
	void *payload)
{
	git_oid *oid_p;
	khiter_t pos;

	assert(importer && mark);

	pos = kh_get(mark, importer->markmap, mark);
	if (pos == kh_end(importer->markmap))
		return -1;
	oid_p = &kh_val(importer->markmap, pos);

	return git_importer_cat_blob_from_oid(importer, oid_p, cb, payload);
}

