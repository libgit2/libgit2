/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#include "iter.h"

#include "system.h"

#include "block.h"
#include "constants.h"
#include "reader.h"
#include "reftable.h"

int iterator_is_null(struct reftable_iterator *it)
{
	return it->ops == NULL;
}

static int empty_iterator_next(void *arg, struct reftable_record *rec)
{
	return 1;
}

static void empty_iterator_close(void *arg)
{
}

static struct reftable_iterator_vtable empty_vtable = {
	.next = &empty_iterator_next,
	.close = &empty_iterator_close,
};

void iterator_set_empty(struct reftable_iterator *it)
{
	assert(it->ops == NULL);
	it->iter_arg = NULL;
	it->ops = &empty_vtable;
}

int iterator_next(struct reftable_iterator *it, struct reftable_record *rec)
{
	return it->ops->next(it->iter_arg, rec);
}

void reftable_iterator_destroy(struct reftable_iterator *it)
{
	if (it->ops == NULL) {
		return;
	}
	it->ops->close(it->iter_arg);
	it->ops = NULL;
	FREE_AND_NULL(it->iter_arg);
}

int reftable_iterator_next_ref(struct reftable_iterator *it,
			       struct reftable_ref_record *ref)
{
	struct reftable_record rec = { 0 };
	reftable_record_from_ref(&rec, ref);
	return iterator_next(it, &rec);
}

int reftable_iterator_next_log(struct reftable_iterator *it,
			       struct reftable_log_record *log)
{
	struct reftable_record rec = { 0 };
	reftable_record_from_log(&rec, log);
	return iterator_next(it, &rec);
}

static void filtering_ref_iterator_close(void *iter_arg)
{
	struct filtering_ref_iterator *fri =
		(struct filtering_ref_iterator *)iter_arg;
	strbuf_release(&fri->oid);
	reftable_iterator_destroy(&fri->it);
}

static int filtering_ref_iterator_next(void *iter_arg,
				       struct reftable_record *rec)
{
	struct filtering_ref_iterator *fri =
		(struct filtering_ref_iterator *)iter_arg;
	struct reftable_ref_record *ref =
		(struct reftable_ref_record *)rec->data;
	int err = 0;
	while (1) {
		err = reftable_iterator_next_ref(&fri->it, ref);
		if (err != 0) {
			break;
		}

		if (fri->double_check) {
			struct reftable_iterator it = { 0 };

			err = reftable_table_seek_ref(&fri->tab, &it,
						      ref->refname);
			if (err == 0) {
				err = reftable_iterator_next_ref(&it, ref);
			}

			reftable_iterator_destroy(&it);

			if (err < 0) {
				break;
			}

			if (err > 0) {
				continue;
			}
		}

		if ((ref->target_value != NULL &&
		     !memcmp(fri->oid.buf, ref->target_value, fri->oid.len)) ||
		    (ref->value != NULL &&
		     !memcmp(fri->oid.buf, ref->value, fri->oid.len))) {
			return 0;
		}
	}

	reftable_ref_record_clear(ref);
	return err;
}

static struct reftable_iterator_vtable filtering_ref_iterator_vtable = {
	.next = &filtering_ref_iterator_next,
	.close = &filtering_ref_iterator_close,
};

void iterator_from_filtering_ref_iterator(struct reftable_iterator *it,
					  struct filtering_ref_iterator *fri)
{
	assert(it->ops == NULL);
	it->iter_arg = fri;
	it->ops = &filtering_ref_iterator_vtable;
}

static void indexed_table_ref_iter_close(void *p)
{
	struct indexed_table_ref_iter *it = (struct indexed_table_ref_iter *)p;
	block_iter_close(&it->cur);
	reftable_block_done(&it->block_reader.block);
	strbuf_release(&it->oid);
}

static int indexed_table_ref_iter_next_block(struct indexed_table_ref_iter *it)
{
	uint64_t off;
	int err = 0;
	if (it->offset_idx == it->offset_len) {
		it->is_finished = 1;
		return 1;
	}

	reftable_block_done(&it->block_reader.block);

	off = it->offsets[it->offset_idx++];
	err = reader_init_block_reader(it->r, &it->block_reader, off,
				       BLOCK_TYPE_REF);
	if (err < 0) {
		return err;
	}
	if (err > 0) {
		/* indexed block does not exist. */
		return REFTABLE_FORMAT_ERROR;
	}
	block_reader_start(&it->block_reader, &it->cur);
	return 0;
}

static int indexed_table_ref_iter_next(void *p, struct reftable_record *rec)
{
	struct indexed_table_ref_iter *it = (struct indexed_table_ref_iter *)p;
	struct reftable_ref_record *ref =
		(struct reftable_ref_record *)rec->data;

	while (1) {
		int err = block_iter_next(&it->cur, rec);
		if (err < 0) {
			return err;
		}

		if (err > 0) {
			err = indexed_table_ref_iter_next_block(it);
			if (err < 0) {
				return err;
			}

			if (it->is_finished) {
				return 1;
			}
			continue;
		}

		if (!memcmp(it->oid.buf, ref->target_value, it->oid.len) ||
		    !memcmp(it->oid.buf, ref->value, it->oid.len)) {
			return 0;
		}
	}
}

int new_indexed_table_ref_iter(struct indexed_table_ref_iter **dest,
			       struct reftable_reader *r, uint8_t *oid,
			       int oid_len, uint64_t *offsets, int offset_len)
{
	struct indexed_table_ref_iter empty = INDEXED_TABLE_REF_ITER_INIT;
	struct indexed_table_ref_iter *itr =
		reftable_calloc(sizeof(struct indexed_table_ref_iter));
	int err = 0;

	*itr = empty;
	itr->r = r;
	strbuf_add(&itr->oid, oid, oid_len);

	itr->offsets = offsets;
	itr->offset_len = offset_len;

	err = indexed_table_ref_iter_next_block(itr);
	if (err < 0) {
		reftable_free(itr);
	} else {
		*dest = itr;
	}
	return err;
}

static struct reftable_iterator_vtable indexed_table_ref_iter_vtable = {
	.next = &indexed_table_ref_iter_next,
	.close = &indexed_table_ref_iter_close,
};

void iterator_from_indexed_table_ref_iter(struct reftable_iterator *it,
					  struct indexed_table_ref_iter *itr)
{
	assert(it->ops == NULL);
	it->iter_arg = itr;
	it->ops = &indexed_table_ref_iter_vtable;
}
