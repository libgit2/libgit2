/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

/* record.c - methods for different types of records. */

#include "record.h"

#include "system.h"
#include "constants.h"
#include "reftable.h"
#include "basics.h"

int get_var_int(uint64_t *dest, struct string_view *in)
{
	int ptr = 0;
	uint64_t val;

	if (in->len == 0)
		return -1;
	val = in->buf[ptr] & 0x7f;

	while (in->buf[ptr] & 0x80) {
		ptr++;
		if (ptr > in->len) {
			return -1;
		}
		val = (val + 1) << 7 | (uint64_t)(in->buf[ptr] & 0x7f);
	}

	*dest = val;
	return ptr + 1;
}

int put_var_int(struct string_view *dest, uint64_t val)
{
	uint8_t buf[10] = { 0 };
	int i = 9;
	int n = 0;
	buf[i] = (uint8_t)(val & 0x7f);
	i--;
	while (1) {
		val >>= 7;
		if (!val) {
			break;
		}
		val--;
		buf[i] = 0x80 | (uint8_t)(val & 0x7f);
		i--;
	}

	n = sizeof(buf) - i - 1;
	if (dest->len < n)
		return -1;
	memcpy(dest->buf, &buf[i + 1], n);
	return n;
}

int reftable_is_block_type(uint8_t typ)
{
	switch (typ) {
	case BLOCK_TYPE_REF:
	case BLOCK_TYPE_LOG:
	case BLOCK_TYPE_OBJ:
	case BLOCK_TYPE_INDEX:
		return 1;
	}
	return 0;
}

static int decode_string(struct strbuf *dest, struct string_view in)
{
	int start_len = in.len;
	uint64_t tsize = 0;
	int n = get_var_int(&tsize, &in);
	if (n <= 0)
		return -1;
	string_view_consume(&in, n);
	if (in.len < tsize)
		return -1;

	strbuf_reset(dest);
	strbuf_add(dest, in.buf, tsize);
	string_view_consume(&in, tsize);

	return start_len - in.len;
}

static int encode_string(char *str, struct string_view s)
{
	struct string_view start = s;
	int l = strlen(str);
	int n = put_var_int(&s, l);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);
	if (s.len < l)
		return -1;
	memcpy(s.buf, str, l);
	string_view_consume(&s, l);

	return start.len - s.len;
}

int reftable_encode_key(int *restart, struct string_view dest,
			struct strbuf prev_key, struct strbuf key,
			uint8_t extra)
{
	struct string_view start = dest;
	int prefix_len = common_prefix_size(&prev_key, &key);
	uint64_t suffix_len = key.len - prefix_len;
	int n = put_var_int(&dest, (uint64_t)prefix_len);
	if (n < 0)
		return -1;
	string_view_consume(&dest, n);

	*restart = (prefix_len == 0);

	n = put_var_int(&dest, suffix_len << 3 | (uint64_t)extra);
	if (n < 0)
		return -1;
	string_view_consume(&dest, n);

	if (dest.len < suffix_len)
		return -1;
	memcpy(dest.buf, key.buf + prefix_len, suffix_len);
	string_view_consume(&dest, suffix_len);

	return start.len - dest.len;
}

int reftable_decode_key(struct strbuf *key, uint8_t *extra,
			struct strbuf last_key, struct string_view in)
{
	int start_len = in.len;
	uint64_t prefix_len = 0;
	uint64_t suffix_len = 0;
	int n = get_var_int(&prefix_len, &in);
	if (n < 0)
		return -1;
	string_view_consume(&in, n);

	if (prefix_len > last_key.len)
		return -1;

	n = get_var_int(&suffix_len, &in);
	if (n <= 0)
		return -1;
	string_view_consume(&in, n);

	*extra = (uint8_t)(suffix_len & 0x7);
	suffix_len >>= 3;

	if (in.len < suffix_len)
		return -1;

	strbuf_reset(key);
	strbuf_add(key, last_key.buf, prefix_len);
	strbuf_add(key, in.buf, suffix_len);
	string_view_consume(&in, suffix_len);

	return start_len - in.len;
}

static void reftable_ref_record_key(const void *r, struct strbuf *dest)
{
	const struct reftable_ref_record *rec =
		(const struct reftable_ref_record *)r;
	strbuf_reset(dest);
	strbuf_addstr(dest, rec->refname);
}

static void reftable_ref_record_copy_from(void *rec, const void *src_rec,
					  int hash_size)
{
	struct reftable_ref_record *ref = (struct reftable_ref_record *)rec;
	struct reftable_ref_record *src = (struct reftable_ref_record *)src_rec;
	assert(hash_size > 0);

	/* This is simple and correct, but we could probably reuse the hash
	   fields. */
	reftable_ref_record_clear(ref);
	if (src->refname != NULL) {
		ref->refname = xstrdup(src->refname);
	}

	if (src->target != NULL) {
		ref->target = xstrdup(src->target);
	}

	if (src->target_value != NULL) {
		ref->target_value = reftable_malloc(hash_size);
		memcpy(ref->target_value, src->target_value, hash_size);
	}

	if (src->value != NULL) {
		ref->value = reftable_malloc(hash_size);
		memcpy(ref->value, src->value, hash_size);
	}
	ref->update_index = src->update_index;
}

static char hexdigit(int c)
{
	if (c <= 9)
		return '0' + c;
	return 'a' + (c - 10);
}

static void hex_format(char *dest, uint8_t *src, int hash_size)
{
	assert(hash_size > 0);
	if (src != NULL) {
		int i = 0;
		for (i = 0; i < hash_size; i++) {
			dest[2 * i] = hexdigit(src[i] >> 4);
			dest[2 * i + 1] = hexdigit(src[i] & 0xf);
		}
		dest[2 * hash_size] = 0;
	}
}

void reftable_ref_record_print(struct reftable_ref_record *ref,
			       uint32_t hash_id)
{
	char hex[SHA256_SIZE + 1] = { 0 };
	printf("ref{%s(%" PRIu64 ") ", ref->refname, ref->update_index);
	if (ref->value != NULL) {
		hex_format(hex, ref->value, hash_size(hash_id));
		printf("%s", hex);
	}
	if (ref->target_value != NULL) {
		hex_format(hex, ref->target_value, hash_size(hash_id));
		printf(" (T %s)", hex);
	}
	if (ref->target != NULL) {
		printf("=> %s", ref->target);
	}
	printf("}\n");
}

static void reftable_ref_record_clear_void(void *rec)
{
	reftable_ref_record_clear((struct reftable_ref_record *)rec);
}

void reftable_ref_record_clear(struct reftable_ref_record *ref)
{
	reftable_free(ref->refname);
	reftable_free(ref->target);
	reftable_free(ref->target_value);
	reftable_free(ref->value);
	memset(ref, 0, sizeof(struct reftable_ref_record));
}

static uint8_t reftable_ref_record_val_type(const void *rec)
{
	const struct reftable_ref_record *r =
		(const struct reftable_ref_record *)rec;
	if (r->value != NULL) {
		if (r->target_value != NULL) {
			return 2;
		} else {
			return 1;
		}
	} else if (r->target != NULL)
		return 3;
	return 0;
}

static int reftable_ref_record_encode(const void *rec, struct string_view s,
				      int hash_size)
{
	const struct reftable_ref_record *r =
		(const struct reftable_ref_record *)rec;
	struct string_view start = s;
	int n = put_var_int(&s, r->update_index);
	assert(hash_size > 0);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	if (r->value != NULL) {
		if (s.len < hash_size) {
			return -1;
		}
		memcpy(s.buf, r->value, hash_size);
		string_view_consume(&s, hash_size);
	}

	if (r->target_value != NULL) {
		if (s.len < hash_size) {
			return -1;
		}
		memcpy(s.buf, r->target_value, hash_size);
		string_view_consume(&s, hash_size);
	}

	if (r->target != NULL) {
		int n = encode_string(r->target, s);
		if (n < 0) {
			return -1;
		}
		string_view_consume(&s, n);
	}

	return start.len - s.len;
}

static int reftable_ref_record_decode(void *rec, struct strbuf key,
				      uint8_t val_type, struct string_view in,
				      int hash_size)
{
	struct reftable_ref_record *r = (struct reftable_ref_record *)rec;
	struct string_view start = in;
	int seen_value = 0;
	int seen_target_value = 0;
	int seen_target = 0;

	int n = get_var_int(&r->update_index, &in);
	if (n < 0)
		return n;
	assert(hash_size > 0);

	string_view_consume(&in, n);

	r->refname = reftable_realloc(r->refname, key.len + 1);
	memcpy(r->refname, key.buf, key.len);
	r->refname[key.len] = 0;

	switch (val_type) {
	case 1:
	case 2:
		if (in.len < hash_size) {
			return -1;
		}

		if (r->value == NULL) {
			r->value = reftable_malloc(hash_size);
		}
		seen_value = 1;
		memcpy(r->value, in.buf, hash_size);
		string_view_consume(&in, hash_size);
		if (val_type == 1) {
			break;
		}
		if (r->target_value == NULL) {
			r->target_value = reftable_malloc(hash_size);
		}
		seen_target_value = 1;
		memcpy(r->target_value, in.buf, hash_size);
		string_view_consume(&in, hash_size);
		break;
	case 3: {
		struct strbuf dest = STRBUF_INIT;
		int n = decode_string(&dest, in);
		if (n < 0) {
			return -1;
		}
		string_view_consume(&in, n);
		seen_target = 1;
		if (r->target != NULL) {
			reftable_free(r->target);
		}
		r->target = dest.buf;
	} break;

	case 0:
		break;
	default:
		abort();
		break;
	}

	if (!seen_target && r->target != NULL) {
		FREE_AND_NULL(r->target);
	}
	if (!seen_target_value && r->target_value != NULL) {
		FREE_AND_NULL(r->target_value);
	}
	if (!seen_value && r->value != NULL) {
		FREE_AND_NULL(r->value);
	}

	return start.len - in.len;
}

static int reftable_ref_record_is_deletion_void(const void *p)
{
	return reftable_ref_record_is_deletion(
		(const struct reftable_ref_record *)p);
}

static struct reftable_record_vtable reftable_ref_record_vtable = {
	.key = &reftable_ref_record_key,
	.type = BLOCK_TYPE_REF,
	.copy_from = &reftable_ref_record_copy_from,
	.val_type = &reftable_ref_record_val_type,
	.encode = &reftable_ref_record_encode,
	.decode = &reftable_ref_record_decode,
	.clear = &reftable_ref_record_clear_void,
	.is_deletion = &reftable_ref_record_is_deletion_void,
};

static void reftable_obj_record_key(const void *r, struct strbuf *dest)
{
	const struct reftable_obj_record *rec =
		(const struct reftable_obj_record *)r;
	strbuf_reset(dest);
	strbuf_add(dest, rec->hash_prefix, rec->hash_prefix_len);
}

static void reftable_obj_record_clear(void *rec)
{
	struct reftable_obj_record *obj = (struct reftable_obj_record *)rec;
	FREE_AND_NULL(obj->hash_prefix);
	FREE_AND_NULL(obj->offsets);
	memset(obj, 0, sizeof(struct reftable_obj_record));
}

static void reftable_obj_record_copy_from(void *rec, const void *src_rec,
					  int hash_size)
{
	struct reftable_obj_record *obj = (struct reftable_obj_record *)rec;
	const struct reftable_obj_record *src =
		(const struct reftable_obj_record *)src_rec;
	int olen;

	reftable_obj_record_clear(obj);
	*obj = *src;
	obj->hash_prefix = reftable_malloc(obj->hash_prefix_len);
	memcpy(obj->hash_prefix, src->hash_prefix, obj->hash_prefix_len);

	olen = obj->offset_len * sizeof(uint64_t);
	obj->offsets = reftable_malloc(olen);
	memcpy(obj->offsets, src->offsets, olen);
}

static uint8_t reftable_obj_record_val_type(const void *rec)
{
	struct reftable_obj_record *r = (struct reftable_obj_record *)rec;
	if (r->offset_len > 0 && r->offset_len < 8)
		return r->offset_len;
	return 0;
}

static int reftable_obj_record_encode(const void *rec, struct string_view s,
				      int hash_size)
{
	struct reftable_obj_record *r = (struct reftable_obj_record *)rec;
	struct string_view start = s;
	int i = 0;
	int n = 0;
	uint64_t last = 0;
	if (r->offset_len == 0 || r->offset_len >= 8) {
		n = put_var_int(&s, r->offset_len);
		if (n < 0) {
			return -1;
		}
		string_view_consume(&s, n);
	}
	if (r->offset_len == 0)
		return start.len - s.len;
	n = put_var_int(&s, r->offsets[0]);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	last = r->offsets[0];
	for (i = 1; i < r->offset_len; i++) {
		int n = put_var_int(&s, r->offsets[i] - last);
		if (n < 0) {
			return -1;
		}
		string_view_consume(&s, n);
		last = r->offsets[i];
	}
	return start.len - s.len;
}

static int reftable_obj_record_decode(void *rec, struct strbuf key,
				      uint8_t val_type, struct string_view in,
				      int hash_size)
{
	struct string_view start = in;
	struct reftable_obj_record *r = (struct reftable_obj_record *)rec;
	uint64_t count = val_type;
	int n = 0;
	uint64_t last;
	int j;
	r->hash_prefix = reftable_malloc(key.len);
	memcpy(r->hash_prefix, key.buf, key.len);
	r->hash_prefix_len = key.len;

	if (val_type == 0) {
		n = get_var_int(&count, &in);
		if (n < 0) {
			return n;
		}

		string_view_consume(&in, n);
	}

	r->offsets = NULL;
	r->offset_len = 0;
	if (count == 0)
		return start.len - in.len;

	r->offsets = reftable_malloc(count * sizeof(uint64_t));
	r->offset_len = count;

	n = get_var_int(&r->offsets[0], &in);
	if (n < 0)
		return n;
	string_view_consume(&in, n);

	last = r->offsets[0];
	j = 1;
	while (j < count) {
		uint64_t delta = 0;
		int n = get_var_int(&delta, &in);
		if (n < 0) {
			return n;
		}
		string_view_consume(&in, n);

		last = r->offsets[j] = (delta + last);
		j++;
	}
	return start.len - in.len;
}

static int not_a_deletion(const void *p)
{
	return 0;
}

static struct reftable_record_vtable reftable_obj_record_vtable = {
	.key = &reftable_obj_record_key,
	.type = BLOCK_TYPE_OBJ,
	.copy_from = &reftable_obj_record_copy_from,
	.val_type = &reftable_obj_record_val_type,
	.encode = &reftable_obj_record_encode,
	.decode = &reftable_obj_record_decode,
	.clear = &reftable_obj_record_clear,
	.is_deletion = not_a_deletion,
};

void reftable_log_record_print(struct reftable_log_record *log,
			       uint32_t hash_id)
{
	char hex[SHA256_SIZE + 1] = { 0 };

	printf("log{%s(%" PRIu64 ") %s <%s> %" PRIu64 " %04d\n", log->refname,
	       log->update_index, log->name, log->email, log->time,
	       log->tz_offset);
	hex_format(hex, log->old_hash, hash_size(hash_id));
	printf("%s => ", hex);
	hex_format(hex, log->new_hash, hash_size(hash_id));
	printf("%s\n\n%s\n}\n", hex, log->message);
}

static void reftable_log_record_key(const void *r, struct strbuf *dest)
{
	const struct reftable_log_record *rec =
		(const struct reftable_log_record *)r;
	int len = strlen(rec->refname);
	uint8_t i64[8];
	uint64_t ts = 0;
	strbuf_reset(dest);
	strbuf_add(dest, (uint8_t *)rec->refname, len + 1);

	ts = (~ts) - rec->update_index;
	put_be64(&i64[0], ts);
	strbuf_add(dest, i64, sizeof(i64));
}

static void reftable_log_record_copy_from(void *rec, const void *src_rec,
					  int hash_size)
{
	struct reftable_log_record *dst = (struct reftable_log_record *)rec;
	const struct reftable_log_record *src =
		(const struct reftable_log_record *)src_rec;

	reftable_log_record_clear(dst);
	*dst = *src;
	if (dst->refname != NULL) {
		dst->refname = xstrdup(dst->refname);
	}
	if (dst->email != NULL) {
		dst->email = xstrdup(dst->email);
	}
	if (dst->name != NULL) {
		dst->name = xstrdup(dst->name);
	}
	if (dst->message != NULL) {
		dst->message = xstrdup(dst->message);
	}

	if (dst->new_hash != NULL) {
		dst->new_hash = reftable_malloc(hash_size);
		memcpy(dst->new_hash, src->new_hash, hash_size);
	}
	if (dst->old_hash != NULL) {
		dst->old_hash = reftable_malloc(hash_size);
		memcpy(dst->old_hash, src->old_hash, hash_size);
	}
}

static void reftable_log_record_clear_void(void *rec)
{
	struct reftable_log_record *r = (struct reftable_log_record *)rec;
	reftable_log_record_clear(r);
}

void reftable_log_record_clear(struct reftable_log_record *r)
{
	reftable_free(r->refname);
	reftable_free(r->new_hash);
	reftable_free(r->old_hash);
	reftable_free(r->name);
	reftable_free(r->email);
	reftable_free(r->message);
	memset(r, 0, sizeof(struct reftable_log_record));
}

static uint8_t reftable_log_record_val_type(const void *rec)
{
	const struct reftable_log_record *log =
		(const struct reftable_log_record *)rec;

	return reftable_log_record_is_deletion(log) ? 0 : 1;
}

static uint8_t zero[SHA256_SIZE] = { 0 };

static int reftable_log_record_encode(const void *rec, struct string_view s,
				      int hash_size)
{
	struct reftable_log_record *r = (struct reftable_log_record *)rec;
	struct string_view start = s;
	int n = 0;
	uint8_t *oldh = r->old_hash;
	uint8_t *newh = r->new_hash;
	if (reftable_log_record_is_deletion(r))
		return 0;

	if (oldh == NULL) {
		oldh = zero;
	}
	if (newh == NULL) {
		newh = zero;
	}

	if (s.len < 2 * hash_size)
		return -1;

	memcpy(s.buf, oldh, hash_size);
	memcpy(s.buf + hash_size, newh, hash_size);
	string_view_consume(&s, 2 * hash_size);

	n = encode_string(r->name ? r->name : "", s);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	n = encode_string(r->email ? r->email : "", s);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	n = put_var_int(&s, r->time);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	if (s.len < 2)
		return -1;

	put_be16(s.buf, r->tz_offset);
	string_view_consume(&s, 2);

	n = encode_string(r->message ? r->message : "", s);
	if (n < 0)
		return -1;
	string_view_consume(&s, n);

	return start.len - s.len;
}

static int reftable_log_record_decode(void *rec, struct strbuf key,
				      uint8_t val_type, struct string_view in,
				      int hash_size)
{
	struct string_view start = in;
	struct reftable_log_record *r = (struct reftable_log_record *)rec;
	uint64_t max = 0;
	uint64_t ts = 0;
	struct strbuf dest = STRBUF_INIT;
	int n;

	if (key.len <= 9 || key.buf[key.len - 9] != 0)
		return REFTABLE_FORMAT_ERROR;

	r->refname = reftable_realloc(r->refname, key.len - 8);
	memcpy(r->refname, key.buf, key.len - 8);
	ts = get_be64(key.buf + key.len - 8);

	r->update_index = (~max) - ts;

	if (val_type == 0) {
		FREE_AND_NULL(r->old_hash);
		FREE_AND_NULL(r->new_hash);
		FREE_AND_NULL(r->message);
		FREE_AND_NULL(r->email);
		FREE_AND_NULL(r->name);
		return 0;
	}

	if (in.len < 2 * hash_size)
		return REFTABLE_FORMAT_ERROR;

	r->old_hash = reftable_realloc(r->old_hash, hash_size);
	r->new_hash = reftable_realloc(r->new_hash, hash_size);

	memcpy(r->old_hash, in.buf, hash_size);
	memcpy(r->new_hash, in.buf + hash_size, hash_size);

	string_view_consume(&in, 2 * hash_size);

	n = decode_string(&dest, in);
	if (n < 0)
		goto done;
	string_view_consume(&in, n);

	r->name = reftable_realloc(r->name, dest.len + 1);
	memcpy(r->name, dest.buf, dest.len);
	r->name[dest.len] = 0;

	strbuf_reset(&dest);
	n = decode_string(&dest, in);
	if (n < 0)
		goto done;
	string_view_consume(&in, n);

	r->email = reftable_realloc(r->email, dest.len + 1);
	memcpy(r->email, dest.buf, dest.len);
	r->email[dest.len] = 0;

	ts = 0;
	n = get_var_int(&ts, &in);
	if (n < 0)
		goto done;
	string_view_consume(&in, n);
	r->time = ts;
	if (in.len < 2)
		goto done;

	r->tz_offset = get_be16(in.buf);
	string_view_consume(&in, 2);

	strbuf_reset(&dest);
	n = decode_string(&dest, in);
	if (n < 0)
		goto done;
	string_view_consume(&in, n);

	r->message = reftable_realloc(r->message, dest.len + 1);
	memcpy(r->message, dest.buf, dest.len);
	r->message[dest.len] = 0;

	strbuf_release(&dest);
	return start.len - in.len;

done:
	strbuf_release(&dest);
	return REFTABLE_FORMAT_ERROR;
}

static int null_streq(char *a, char *b)
{
	char *empty = "";
	if (a == NULL)
		a = empty;

	if (b == NULL)
		b = empty;

	return 0 == strcmp(a, b);
}

static int zero_hash_eq(uint8_t *a, uint8_t *b, int sz)
{
	if (a == NULL)
		a = zero;

	if (b == NULL)
		b = zero;

	return !memcmp(a, b, sz);
}

int reftable_log_record_equal(struct reftable_log_record *a,
			      struct reftable_log_record *b, int hash_size)
{
	return null_streq(a->name, b->name) && null_streq(a->email, b->email) &&
	       null_streq(a->message, b->message) &&
	       zero_hash_eq(a->old_hash, b->old_hash, hash_size) &&
	       zero_hash_eq(a->new_hash, b->new_hash, hash_size) &&
	       a->time == b->time && a->tz_offset == b->tz_offset &&
	       a->update_index == b->update_index;
}

static int reftable_log_record_is_deletion_void(const void *p)
{
	return reftable_log_record_is_deletion(
		(const struct reftable_log_record *)p);
}

static struct reftable_record_vtable reftable_log_record_vtable = {
	.key = &reftable_log_record_key,
	.type = BLOCK_TYPE_LOG,
	.copy_from = &reftable_log_record_copy_from,
	.val_type = &reftable_log_record_val_type,
	.encode = &reftable_log_record_encode,
	.decode = &reftable_log_record_decode,
	.clear = &reftable_log_record_clear_void,
	.is_deletion = &reftable_log_record_is_deletion_void,
};

struct reftable_record reftable_new_record(uint8_t typ)
{
	struct reftable_record rec = { NULL };
	switch (typ) {
	case BLOCK_TYPE_REF: {
		struct reftable_ref_record *r =
			reftable_calloc(sizeof(struct reftable_ref_record));
		reftable_record_from_ref(&rec, r);
		return rec;
	}

	case BLOCK_TYPE_OBJ: {
		struct reftable_obj_record *r =
			reftable_calloc(sizeof(struct reftable_obj_record));
		reftable_record_from_obj(&rec, r);
		return rec;
	}
	case BLOCK_TYPE_LOG: {
		struct reftable_log_record *r =
			reftable_calloc(sizeof(struct reftable_log_record));
		reftable_record_from_log(&rec, r);
		return rec;
	}
	case BLOCK_TYPE_INDEX: {
		struct reftable_index_record empty = { .last_key =
							       STRBUF_INIT };
		struct reftable_index_record *r =
			reftable_calloc(sizeof(struct reftable_index_record));
		*r = empty;
		reftable_record_from_index(&rec, r);
		return rec;
	}
	}
	abort();
	return rec;
}

/* clear out the record, yielding the reftable_record data that was
 * encapsulated. */
static void *reftable_record_yield(struct reftable_record *rec)
{
	void *p = rec->data;
	rec->data = NULL;
	return p;
}

void reftable_record_destroy(struct reftable_record *rec)
{
	reftable_record_clear(rec);
	reftable_free(reftable_record_yield(rec));
}

static void reftable_index_record_key(const void *r, struct strbuf *dest)
{
	struct reftable_index_record *rec = (struct reftable_index_record *)r;
	strbuf_reset(dest);
	strbuf_addbuf(dest, &rec->last_key);
}

static void reftable_index_record_copy_from(void *rec, const void *src_rec,
					    int hash_size)
{
	struct reftable_index_record *dst = (struct reftable_index_record *)rec;
	struct reftable_index_record *src =
		(struct reftable_index_record *)src_rec;

	strbuf_reset(&dst->last_key);
	strbuf_addbuf(&dst->last_key, &src->last_key);
	dst->offset = src->offset;
}

static void reftable_index_record_clear(void *rec)
{
	struct reftable_index_record *idx = (struct reftable_index_record *)rec;
	strbuf_release(&idx->last_key);
}

static uint8_t reftable_index_record_val_type(const void *rec)
{
	return 0;
}

static int reftable_index_record_encode(const void *rec, struct string_view out,
					int hash_size)
{
	const struct reftable_index_record *r =
		(const struct reftable_index_record *)rec;
	struct string_view start = out;

	int n = put_var_int(&out, r->offset);
	if (n < 0)
		return n;

	string_view_consume(&out, n);

	return start.len - out.len;
}

static int reftable_index_record_decode(void *rec, struct strbuf key,
					uint8_t val_type, struct string_view in,
					int hash_size)
{
	struct string_view start = in;
	struct reftable_index_record *r = (struct reftable_index_record *)rec;
	int n = 0;

	strbuf_reset(&r->last_key);
	strbuf_addbuf(&r->last_key, &key);

	n = get_var_int(&r->offset, &in);
	if (n < 0)
		return n;

	string_view_consume(&in, n);
	return start.len - in.len;
}

static struct reftable_record_vtable reftable_index_record_vtable = {
	.key = &reftable_index_record_key,
	.type = BLOCK_TYPE_INDEX,
	.copy_from = &reftable_index_record_copy_from,
	.val_type = &reftable_index_record_val_type,
	.encode = &reftable_index_record_encode,
	.decode = &reftable_index_record_decode,
	.clear = &reftable_index_record_clear,
	.is_deletion = &not_a_deletion,
};

void reftable_record_key(struct reftable_record *rec, struct strbuf *dest)
{
	rec->ops->key(rec->data, dest);
}

uint8_t reftable_record_type(struct reftable_record *rec)
{
	return rec->ops->type;
}

int reftable_record_encode(struct reftable_record *rec, struct string_view dest,
			   int hash_size)
{
	return rec->ops->encode(rec->data, dest, hash_size);
}

void reftable_record_copy_from(struct reftable_record *rec,
			       struct reftable_record *src, int hash_size)
{
	assert(src->ops->type == rec->ops->type);

	rec->ops->copy_from(rec->data, src->data, hash_size);
}

uint8_t reftable_record_val_type(struct reftable_record *rec)
{
	return rec->ops->val_type(rec->data);
}

int reftable_record_decode(struct reftable_record *rec, struct strbuf key,
			   uint8_t extra, struct string_view src, int hash_size)
{
	return rec->ops->decode(rec->data, key, extra, src, hash_size);
}

void reftable_record_clear(struct reftable_record *rec)
{
	rec->ops->clear(rec->data);
}

int reftable_record_is_deletion(struct reftable_record *rec)
{
	return rec->ops->is_deletion(rec->data);
}

void reftable_record_from_ref(struct reftable_record *rec,
			      struct reftable_ref_record *ref_rec)
{
	assert(rec->ops == NULL);
	rec->data = ref_rec;
	rec->ops = &reftable_ref_record_vtable;
}

void reftable_record_from_obj(struct reftable_record *rec,
			      struct reftable_obj_record *obj_rec)
{
	assert(rec->ops == NULL);
	rec->data = obj_rec;
	rec->ops = &reftable_obj_record_vtable;
}

void reftable_record_from_index(struct reftable_record *rec,
				struct reftable_index_record *index_rec)
{
	assert(rec->ops == NULL);
	rec->data = index_rec;
	rec->ops = &reftable_index_record_vtable;
}

void reftable_record_from_log(struct reftable_record *rec,
			      struct reftable_log_record *log_rec)
{
	assert(rec->ops == NULL);
	rec->data = log_rec;
	rec->ops = &reftable_log_record_vtable;
}

struct reftable_ref_record *reftable_record_as_ref(struct reftable_record *rec)
{
	assert(reftable_record_type(rec) == BLOCK_TYPE_REF);
	return (struct reftable_ref_record *)rec->data;
}

struct reftable_log_record *reftable_record_as_log(struct reftable_record *rec)
{
	assert(reftable_record_type(rec) == BLOCK_TYPE_LOG);
	return (struct reftable_log_record *)rec->data;
}

static int hash_equal(uint8_t *a, uint8_t *b, int hash_size)
{
	if (a != NULL && b != NULL)
		return !memcmp(a, b, hash_size);

	return a == b;
}

static int str_equal(char *a, char *b)
{
	if (a != NULL && b != NULL)
		return 0 == strcmp(a, b);

	return a == b;
}

int reftable_ref_record_equal(struct reftable_ref_record *a,
			      struct reftable_ref_record *b, int hash_size)
{
	assert(hash_size > 0);
	return 0 == strcmp(a->refname, b->refname) &&
	       a->update_index == b->update_index &&
	       hash_equal(a->value, b->value, hash_size) &&
	       hash_equal(a->target_value, b->target_value, hash_size) &&
	       str_equal(a->target, b->target);
}

int reftable_ref_record_compare_name(const void *a, const void *b)
{
	return strcmp(((struct reftable_ref_record *)a)->refname,
		      ((struct reftable_ref_record *)b)->refname);
}

int reftable_ref_record_is_deletion(const struct reftable_ref_record *ref)
{
	return ref->value == NULL && ref->target == NULL &&
	       ref->target_value == NULL;
}

int reftable_log_record_compare_key(const void *a, const void *b)
{
	struct reftable_log_record *la = (struct reftable_log_record *)a;
	struct reftable_log_record *lb = (struct reftable_log_record *)b;

	int cmp = strcmp(la->refname, lb->refname);
	if (cmp)
		return cmp;
	if (la->update_index > lb->update_index)
		return -1;
	return (la->update_index < lb->update_index) ? 1 : 0;
}

int reftable_log_record_is_deletion(const struct reftable_log_record *log)
{
	return (log->new_hash == NULL && log->old_hash == NULL &&
		log->name == NULL && log->email == NULL &&
		log->message == NULL && log->time == 0 && log->tz_offset == 0 &&
		log->message == NULL);
}

void string_view_consume(struct string_view *s, int n)
{
	s->buf += n;
	s->len -= n;
}
