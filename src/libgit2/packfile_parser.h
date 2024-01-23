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

typedef struct {
	/* Callbacks */

	/* Called when the parser has read the packfile's header */
	int (*packfile_header)(uint32_t version, uint32_t entries, void *data);

	/*
	 * The object callbacks are called at the start of each object,
	 * for each chunk of data, and when the object is complete.
	 */
	int (*object_start)(
		git_object_size_t offset,
		uint16_t header_size,
		git_object_t type,
		git_object_size_t size,
		void *data);
	int (*object_data)(void *object_data, size_t len, void *data);
	int (*object_complete)(
		git_object_size_t compressed_size,
		uint32_t compressed_crc,
		git_oid *oid,
		void *data);

	/*
	 * The delta callbacks are called at the start of each ofs or ref
	 * delta, with each byte of data, and when the delta is complete.
	 */
	int (*delta_start)(
		git_object_size_t offset,
		git_object_t type,
		uint16_t header_size,
		git_object_size_t size,
		git_oid *delta_ref,
		git_object_size_t delta_offset,
		void *data);
	int (*delta_data)(void *delta_data, size_t len, void *data);
	int (*delta_complete)(
		git_object_size_t compressed_size,
		uint32_t compressed_crc,
		void *data);

	/* Called when the packfile is completely parsed. */
	int (*packfile_complete)(
		const unsigned char *checksum,
		size_t checksum_len,
		void *data);

	/*
	 * Callback data will be passed as the `data` argument to
	 * callbacks.
	 */
	void *callback_data;

} git_packfile_parser_options;

/**
 * Sets up the packfile parser. Users should set the callbacks
 */
int git_packfile_parser_new(
	git_packfile_parser **out,
	git_oid_t oid_type,
	git_packfile_parser_options *opts);

/**
 * Parse the given chunk of data. As this function parses the given data,
 * it will invoke the appropriate callbacks.
 */
int git_packfile_parser_parse(
	git_packfile_parser *parser,
	const void *data,
	size_t len);

/*
 * Get the hash context for the packfile. Callers may want to mutate the
 * hash - for example, when fixing thin packs.
 */
git_hash_ctx *git_packfile_parser_hash_ctx(git_packfile_parser *parser);

/* Returns true if the parsing is complete. */
bool git_packfile_parser_complete(git_packfile_parser *parser);

/* Disposes the parser. */
void git_packfile_parser_free(git_packfile_parser *parser);

#endif
