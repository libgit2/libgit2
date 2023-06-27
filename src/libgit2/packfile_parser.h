/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_packfile_parser_h__
#define INCLUDE_packfile_parser_h__

#include "oid.h"
#include "pack.h"
#include "repository.h"

#include "git2_util.h"

typedef struct git_packfile_parser git_packfile_parser;

typedef enum {
	STATE_HEADER = 0,
	STATE_OBJECT_HEADER_START,
	STATE_OBJECT_HEADER,
	STATE_OBJECT_DATA_START,
	STATE_OBJECT_DATA,
	STATE_DELTA_HEADER,
	STATE_DELTA_DATA_START,
	STATE_DELTA_DATA,
	STATE_TRAILER,
	STATE_COMPLETE,
	STATE_FAILED
} parser_state;

struct git_packfile_parser {
	git_oid_t oid_type;

	parser_state state;
	size_t position;

	git_zstream zstream;

	/* Callbacks */
	int (*packfile_header)(uint32_t version, uint32_t entries, void *data);
	int (*object_start)(git_object_size_t offset, git_object_size_t header_size, git_object_t type, git_object_size_t size, void *data);
	int (*object_data)(void *object_data, size_t len, void *data);
	int (*object_complete)(git_object_size_t compressed_size, uint32_t compressed_crc, git_oid *oid, void *data);
	int (*delta_start)(git_object_size_t offset, git_object_t type, git_object_size_t header_size, git_object_size_t size, git_oid *delta_ref, git_object_size_t delta_offset, void *data);
	int (*delta_data)(void *delta_data, size_t len, void *data);
	int (*delta_complete)(git_object_size_t compressed_size, uint32_t compressed_crc, void *data);
	int (*packfile_complete)(const unsigned char *checksum, size_t checksum_len, void *data);
	void *callback_data;

	/* Parsing structure for the header */
	struct git_pack_header header;
	size_t header_len;

	/* Parsing structures for the current entry */
	size_t current_idx;
	size_t current_position;

	git_object_t current_type;
	git_object_size_t current_size;
	git_object_size_t current_offset;
	git_object_size_t current_compressed_size;
	uint32_t current_compressed_crc;
	git_oid current_base;
	size_t current_base_len;
	size_t current_bits;
	git_hash_ctx current_hash;

	/* Parsing structure for the trailer */
	unsigned char trailer[GIT_HASH_SHA256_SIZE];
	size_t trailer_len;

	git_hash_ctx packfile_hash;
};

int git_packfile_parser_init(
	git_packfile_parser *parser,
	git_oid_t oid_type);

int git_packfile_parser_parse(
	git_packfile_parser *parser,
	const void *data,
	size_t len);

void git_packfile_parser_dispose(git_packfile_parser *parser);

#endif
