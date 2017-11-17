/* The MIT License

   Copyright (c) 2008, 2009, 2011 by Attractive Chaos <attractor@live.co.uk>

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
*/

#include <stdint.h>
#include <stdlib.h>

#include "khash.h"

#ifndef kcalloc
#define kcalloc(N,Z) calloc(N,Z)
#endif
#ifndef kmalloc
#define kmalloc(Z) malloc(Z)
#endif
#ifndef krealloc
#define krealloc(P,Z) realloc(P,Z)
#endif
#ifndef kreallocarray
#define kreallocarray(P,N,Z) ((SIZE_MAX - N < Z) ? NULL : krealloc(P, (N*Z)))
#endif
#ifndef kfree
#define kfree(P) free(P)
#endif

#define __ac_isempty(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&2)
#define __ac_isdel(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&1)
#define __ac_set_isdel_false(flag, i) (flag[i>>4]&=~(1ul<<((i&0xfU)<<1)))
#define __ac_set_isempty_false(flag, i) (flag[i>>4]&=~(2ul<<((i&0xfU)<<1)))
#define __ac_set_isboth_false(flag, i) (flag[i>>4]&=~(3ul<<((i&0xfU)<<1)))
#define __ac_set_isdel_true(flag, i) (flag[i>>4]|=1ul<<((i&0xfU)<<1))
#define __ac_fsize(m) ((m) < 16? 1 : (m)>>4)

#ifndef kroundup32
#define kroundup32(x) (--(x), (x)|=(x)>>1, (x)|=(x)>>2, (x)|=(x)>>4, (x)|=(x)>>8, (x)|=(x)>>16, ++(x))
#endif

static const double __ac_HASH_UPPER = 0.77;

static void memxchange(void *aptr, void *bptr, size_t len)
{
	char *a = (char *) aptr, *b = (char *) bptr;
	while (len--) {
		*a = *a ^ *b;
		*b = *a ^ *b;
		*a = *a ^ *b;
		a++;
		b++;
	}
}

khash *kh_init(khint_t keysize, khint_t valsize, khash_hash_fn hash, khash_hash_equal_fn hash_equal, bool is_map)
{
	khash *h = (khash*)kcalloc(1, sizeof(khash));
	h->keysize = keysize;
	h->valsize = valsize;
	h->hash = hash;
	h->hash_equal = hash_equal;
	h->is_map = is_map;
	return h;
}

void kh_destroy(khash *h)
{
	if (h) {
		kfree((void *)h->keys); kfree(h->flags);
		kfree((void *)h->vals);
		kfree(h);
	}
}

void kh_clear(khash *h)
{
	if (h && h->flags) {
		memset(h->flags, 0xaa, __ac_fsize(h->n_buckets) * sizeof(khint32_t));
		h->size = h->n_occupied = 0;
	}
}

khint_t kh_get(const khash *h, const void *key)
{
	if (h->n_buckets) {
		khint_t k, i, last, mask, step = 0;
		mask = h->n_buckets - 1;
		k = h->hash(key); i = k & mask;
		last = i;
		while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !h->hash_equal(&kh_key(h, i), key))) {
			i = (i + (++step)) & mask;
			if (i == last) return h->n_buckets;
		}
		return __ac_iseither(h->flags, i)? h->n_buckets : i;
	} else return 0;
}

int kh_resize(khash *h, khint_t new_n_buckets)
{ /* This function uses 0.25*n_buckets bytes of working space instead of [sizeof(key_t+val_t)+.25]*n_buckets. */
	khint32_t *new_flags = 0;
	khint_t j = 1;
	{
		kroundup32(new_n_buckets);
		if (new_n_buckets < 4) new_n_buckets = 4;
		if (h->size >= (khint_t)(new_n_buckets * __ac_HASH_UPPER + 0.5)) j = 0;	/* requested size is too small */
		else { /* hash table size to be changed (shrink or expand); rehash */
			new_flags = (khint32_t*)kreallocarray(NULL, __ac_fsize(new_n_buckets), sizeof(khint32_t));
			if (!new_flags) return -1;
			memset(new_flags, 0xaa, __ac_fsize(new_n_buckets) * sizeof(khint32_t));
			if (h->n_buckets < new_n_buckets) {	/* expand */
				void *new_keys = kreallocarray((void *)h->keys, new_n_buckets, h->keysize);
				if (!new_keys) { kfree(new_flags); return -1; }
				h->keys = new_keys;
				if (h->is_map) {
					void *new_vals = kreallocarray(h->vals, new_n_buckets, h->valsize);
					if (!new_vals) { kfree(new_flags); return -1; }
					h->vals = new_vals;
				}
			} /* otherwise shrink */
		}
	}
	if (j) { /* rehashing is needed */
		for (j = 0; j != h->n_buckets; ++j) {
			if (__ac_iseither(h->flags, j) == 0) {
				khint_t new_mask;
				new_mask = new_n_buckets - 1;
				__ac_set_isdel_true(h->flags, j);
				while (1) { /* kick-out process; sort of like in Cuckoo hashing */
					khint_t k, i, step = 0;
					k = h->hash(&kh_key(h, j));
					i = k & new_mask;
					while (!__ac_isempty(new_flags, i)) i = (i + (++step)) & new_mask;
					__ac_set_isempty_false(new_flags, i);
					if (i < h->n_buckets && __ac_iseither(h->flags, i) == 0) { /* kick out the existing element */
						memxchange(&kh_key(h, i), &kh_key(h, j), h->keysize);
						if (h->is_map) memxchange(&kh_val(h, i), &kh_val(h, j), h->valsize);
						__ac_set_isdel_true(h->flags, i); /* mark it as deleted in the old hash table */
					} else { /* write the element and jump out of the loop */
						memcpy(&kh_key(h, i), &kh_key(h, j), h->keysize);
						if (h->is_map) memcpy(&kh_val(h, i), &kh_val(h, j), h->valsize);
						break;
					}
				}
			}
		}
		if (h->n_buckets > new_n_buckets) { /* shrink the hash table */
			h->keys = kreallocarray((void *)h->keys, new_n_buckets, h->keysize);
			if (h->is_map) h->vals = kreallocarray(h->vals, new_n_buckets, h->valsize);
		}
		kfree(h->flags); /* free the working space */
		h->flags = new_flags;
		h->n_buckets = new_n_buckets;
		h->n_occupied = h->size;
		h->upper_bound = (khint_t)(h->n_buckets * __ac_HASH_UPPER + 0.5);
	}
	return 0;
}

khint_t kh_put(khash *h, const void *key, int *ret)
{
	khint_t x;
	if (h->n_occupied >= h->upper_bound) { /* update the hash table */
		if (h->n_buckets > (h->size<<1)) {
			if (kh_resize(h, h->n_buckets - 1) < 0) { /* clear "deleted" elements */
				*ret = -1; return h->n_buckets;
			}
		} else if (kh_resize(h, h->n_buckets + 1) < 0) { /* expand the hash table */
			*ret = -1; return h->n_buckets;
		}
	} /* TODO: to implement automatically shrinking; resize() already support shrinking */
	{
		khint_t k, i, site, last, mask = h->n_buckets - 1, step = 0;
		x = site = h->n_buckets; k = h->hash(key); i = k & mask;
		if (__ac_isempty(h->flags, i)) x = i; /* for speed up */
		else {
			last = i;
			while (!__ac_isempty(h->flags, i) && (__ac_isdel(h->flags, i) || !h->hash_equal(&kh_key(h, i), key))) {
				if (__ac_isdel(h->flags, i)) site = i;
				i = (i + (++step)) & mask;
				if (i == last) { x = site; break; }
			}
			if (x == h->n_buckets) {
				if (__ac_isempty(h->flags, i) && site != h->n_buckets) x = site;
				else x = i;
			}
		}
	}
	if (__ac_isempty(h->flags, x)) { /* not present at all */
		memcpy(&kh_key(h, x), key, h->keysize);
		__ac_set_isboth_false(h->flags, x);
		++h->size; ++h->n_occupied;
		*ret = 1;
	} else if (__ac_isdel(h->flags, x)) { /* deleted */
		memcpy(&kh_key(h, x), key, h->keysize);
		__ac_set_isboth_false(h->flags, x);
		++h->size;
		*ret = 2;
	} else *ret = 0; /* Don't touch h->keys[x] if present and not deleted */
	return x;
}

void kh_del(khash *h, khint_t x)
{
	if (x != h->n_buckets && !__ac_iseither(h->flags, x)) {
		__ac_set_isdel_true(h->flags, x);
		--h->size;
	}
}

khint_t kh_int_hash_func(const void *key)
{
	return *(khint32_t *) key;
}

int kh_int_hash_equal(const void *a, const void *b)
{
	return *(khint32_t *) a == *(khint32_t *) b;
}

khint_t kh_int64_hash_func(const void *keyptr)
{
	khint64_t key = *(khint64_t *) keyptr;
	return key>>33^key^key<<11;
}

int kh_int64_hash_equal(const void *a, const void *b)
{
	return *(khint64_t *) a == *(khint64_t *) b;
}

khint32_t kh_str_hash_func(const void *ptr)
{
	const char *s = *(const char **) ptr;
	khint_t h = (khint_t)*s;
	if (h) for (++s ; *s; ++s) h = (h << 5) - h + (khint_t)*s;
	return h;
}

int kh_str_hash_equal(const void *a, const void *b)
{
	return strcmp(*(const char **) a, *(const char **) b) == 0;
}
