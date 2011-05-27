#ifndef INCLUDE_sha1_lookup_h__
#define INCLUDE_sha1_lookup_h__

#include <stdlib.h>

int sha1_entry_pos(const void *table,
		   size_t elem_size,
		   size_t key_offset,
		   unsigned lo, unsigned hi, unsigned nr,
		   const unsigned char *key);

#endif
