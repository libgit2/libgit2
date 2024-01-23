/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "packfile_parser.h"

#include "oid.h"
#include "pack.h"
#include "repository.h"

#include "git2_util.h"

#define READ_CHUNK_SIZE (1024 * 256)

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
	git_packfile_parser_options opts;

	parser_state state;
	size_t position;

	git_zstream zstream;

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
	unsigned char trailer[GIT_HASH_MAX_SIZE];
	size_t trailer_len;

	git_hash_ctx packfile_hash;
};

int git_packfile_parser_new(
	git_packfile_parser **out,
	git_oid_t oid_type,
	git_packfile_parser_options *opts)
{
	git_packfile_parser *parser;
	git_hash_algorithm_t checksum_type = git_oid_algorithm(oid_type);

	GIT_ASSERT_ARG(out);
	GIT_ASSERT_ARG(opts);

	parser = git__calloc(1, sizeof(git_packfile_parser));
	GIT_ERROR_CHECK_ALLOC(parser);

	if (opts)
		memcpy(&parser->opts, opts, sizeof(git_packfile_parser_options));

	parser->oid_type = oid_type;

	if (git_zstream_init(&parser->zstream, GIT_ZSTREAM_INFLATE) < 0 ||
	    git_hash_ctx_init(&parser->current_hash, checksum_type) < 0 ||
	    git_hash_ctx_init(&parser->packfile_hash, checksum_type) < 0) {
		git__free(parser);
		return -1;
	}

	*out = parser;
	return 0;
}

static int parse_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *_data,
	size_t len)
{
	const unsigned char *data = (unsigned char *)_data;
	size_t chunk_len = min(len, (sizeof(struct git_pack_header) - parser->header_len));
	size_t orig_len = len;
	unsigned char *header = (unsigned char *)&parser->header;

	memcpy(header + parser->header_len, data, chunk_len);

	data += chunk_len;
	len -= chunk_len;
	parser->header_len += chunk_len;

	if (parser->header_len == sizeof(struct git_pack_header)) {
		parser->header.hdr_signature = ntohl(parser->header.hdr_signature);
		parser->header.hdr_version = ntohl(parser->header.hdr_version);
		parser->header.hdr_entries = ntohl(parser->header.hdr_entries);

		if (parser->header.hdr_signature != PACK_SIGNATURE) {
			git_error_set(GIT_ERROR_INDEXER, "invalid packfile signature");
			return -1;
		}

		if (parser->header.hdr_version != 2) {
			git_error_set(GIT_ERROR_INDEXER, "unsupported packfile version %d", parser->header.hdr_version);
			return -1;
		}

		if (parser->opts.packfile_header) {
			int error = parser->opts.packfile_header(parser->header.hdr_version,
				parser->header.hdr_entries,
				parser->opts.callback_data);

			if (error != 0)
				return error;
		}

		parser->state = parser->header.hdr_entries > 0 ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

#define is_delta(type) \
	((type) == GIT_OBJECT_OFS_DELTA || (type) == GIT_OBJECT_REF_DELTA)

static int parse_object_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *_data,
	size_t len)
{
	const unsigned char *data = (unsigned char *)_data;
	size_t orig_len = len;

	while (len && parser->state < STATE_OBJECT_DATA_START) {
		unsigned char c = *((unsigned char *)data);

		if (parser->state == STATE_OBJECT_HEADER_START) {
			parser->state = STATE_OBJECT_HEADER;
			parser->current_position = parser->position;
			parser->current_type = (c >> 4) & 0x07;
			parser->current_size = c & 0x0f;
			parser->current_compressed_size = 1;
			parser->current_compressed_crc = crc32(0L, Z_NULL, 0);
			parser->current_bits = 4;

			if (git_hash_init(&parser->current_hash) < 0)
				return -1;
		} else {
			parser->current_size += (c & 0x7f) << parser->current_bits;
			parser->current_compressed_size++;
			parser->current_bits += 7;
			GIT_ASSERT(parser->current_bits < 64);
		}

		GIT_ASSERT(parser->current_compressed_size <= UINT16_MAX);

		parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, 1);

		data++;
		len--;

		if ((c & 0x80) == 0 && is_delta(parser->current_type)) {
			parser->current_offset = 0;
			git_oid_clear(&parser->current_base, parser->oid_type);
			parser->current_base_len = 0;
			parser->current_bits = 0;

			parser->state = STATE_DELTA_HEADER;
		} else if ((c & 0x80) == 0) {
			char header[GIT_OBJECT_HEADER_MAX_LEN];
			size_t header_len;

			if (git_odb__format_object_header(&header_len,
					header, sizeof(header),
					parser->current_size,
					parser->current_type) < 0 ||
			    git_hash_update(&parser->current_hash,
					header, header_len) < 0)
				return -1;

			if (parser->opts.object_start) {
				int error = parser->opts.object_start(
					parser->current_position,
					parser->current_compressed_size,
					parser->current_type,
					parser->current_size,
					parser->opts.callback_data);

				if (error != 0)
					return error;
			}

			parser->state = STATE_OBJECT_DATA_START;
		}
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_object_data(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	unsigned char inflated[READ_CHUNK_SIZE];
	size_t inflated_size = READ_CHUNK_SIZE, inflated_len;
	size_t orig_len = len;

	if (parser->state == STATE_OBJECT_DATA_START) {
		git_zstream_reset(&parser->zstream);
		parser->state = STATE_OBJECT_DATA;
	}

	if (git_zstream_set_input(&parser->zstream, data, len) < 0)
		return -1;

	do {
		inflated_len = inflated_size;

		if (git_zstream_get_output_chunk(inflated, &inflated_len, &parser->zstream) < 0 ||
		    git_hash_update(&parser->current_hash, inflated, inflated_len) < 0)
			return -1;

		if (parser->opts.object_data) {
			int error = parser->opts.object_data(
				inflated,
				inflated_len,
				parser->opts.callback_data);

			if (error != 0)
				return error;
		}
	} while (inflated_len > 0);

	len = parser->zstream.in_len;
	parser->current_compressed_size += (orig_len - len);
	parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, (orig_len - len));

	if (git_zstream_eos(&parser->zstream)) {
		git_oid oid = {0};

#ifdef GIT_EXPERIMENTAL_SHA256
		oid.type = parser->oid_type;
#endif

		if (git_hash_final(oid.id, &parser->current_hash) < 0)
			return -1;

		if (parser->opts.object_complete) {
			int error = parser->opts.object_complete(
				parser->current_compressed_size,
				parser->current_compressed_crc,
				&oid,
				parser->opts.callback_data);

			if (error != 0)
				return error;
		}

		parser->state =
			(++parser->current_idx < parser->header.hdr_entries) ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_delta_header(
	size_t *out,
	git_packfile_parser *parser,
	const void *_data,
	size_t len)
{
	const unsigned char *data = (unsigned char *)_data;
	size_t hash_len, chunk_len;
	size_t orig_len = len;
	unsigned char *base;

	switch (parser->current_type) {
	case GIT_OBJECT_OFS_DELTA:
		while (len) {
			char c = *((const char *)data);

			if (parser->current_bits == 0) {
				parser->current_offset = (c & 0x7f);
			} else {
				parser->current_offset += 1;
				parser->current_offset <<= 7;
				parser->current_offset |= (c & 0x7f);
			}

			parser->current_bits += 7;
			GIT_ASSERT(parser->current_bits < 64);

			parser->current_compressed_size++;

			parser->current_compressed_crc =
				crc32(parser->current_compressed_crc, data, 1);

			data++;
			len--;

			if ((c & 0x80) == 0) {
				if (parser->opts.delta_start) {
					int error = parser->opts.delta_start(
						parser->current_position,
						parser->current_type,
						parser->current_compressed_size,
						parser->current_size,
						NULL,
						parser->current_offset,
						parser->opts.callback_data);

					if (error != 0)
						return error;
				}

				parser->state = STATE_DELTA_DATA_START;
				break;
			}
		}

		break;

	case GIT_OBJECT_REF_DELTA:
		hash_len = git_oid_size(parser->oid_type);
		chunk_len = min(hash_len, len);

		base = (unsigned char *)&parser->current_base.id;
		memcpy(base + parser->current_base_len, data, chunk_len);

		parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, chunk_len);

		parser->current_base_len += chunk_len;
		data += chunk_len;
		len -= chunk_len;

		parser->current_compressed_size += chunk_len;
		GIT_ASSERT(parser->current_compressed_size <= UINT16_MAX);

		if (parser->current_base_len == hash_len) {
			if (parser->opts.delta_start) {
				int error = parser->opts.delta_start(
					parser->current_position,
					parser->current_type,
					parser->current_compressed_size,
					parser->current_size,
					&parser->current_base,
					0,
					parser->opts.callback_data);

				if (error != 0)
					return error;
			}

			parser->state = STATE_DELTA_DATA_START;
		}

		break;

	default:
		git_error_set(GIT_ERROR_INDEXER, "invalid delta type");
		return -1;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_delta_data(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	unsigned char inflated[READ_CHUNK_SIZE];
	size_t inflated_len = READ_CHUNK_SIZE;
	size_t orig_len = len;

	if (parser->state == STATE_DELTA_DATA_START) {
		git_zstream_reset(&parser->zstream);
		parser->state = STATE_DELTA_DATA;
	}

	if (git_zstream_set_input(&parser->zstream, data, len) < 0 ||
	    git_zstream_get_output_chunk(inflated, &inflated_len, &parser->zstream) < 0)
		return -1;

	len = parser->zstream.in_len;
	parser->current_compressed_size += (orig_len - len);

	parser->current_compressed_crc =
			crc32(parser->current_compressed_crc, data, (orig_len - len));

	if (parser->opts.delta_data) {
		int error = parser->opts.delta_data(
			inflated,
			inflated_len,
			parser->opts.callback_data);

		if (error != 0)
			return error;
	}

	if (git_zstream_eos(&parser->zstream)) {
		if (parser->opts.delta_complete) {
			int error = parser->opts.delta_complete(
				parser->current_compressed_size,
				parser->current_compressed_crc,
				parser->opts.callback_data);

			if (error != 0)
				return error;
		}

		parser->state =
			(++parser->current_idx < parser->header.hdr_entries) ?
			STATE_OBJECT_HEADER_START :
			STATE_TRAILER;
	}

	*out = (orig_len - len);
	return 0;
}

static int parse_trailer(
	size_t *out,
	git_packfile_parser *parser,
	const void *data,
	size_t len)
{
	git_hash_algorithm_t hash_alg = git_oid_algorithm(parser->oid_type);
	size_t hash_len = git_hash_size(hash_alg);
	size_t chunk_len = min(hash_len, len);
	size_t orig_len = len;

	memcpy(parser->trailer + parser->trailer_len, data, chunk_len);
	parser->trailer_len += chunk_len;

	len -= chunk_len;

	if (parser->trailer_len == hash_len) {
		unsigned char trailer[GIT_HASH_MAX_SIZE];

		git_hash_final(trailer, &parser->packfile_hash);

		if (memcmp(trailer, parser->trailer, parser->trailer_len) != 0) {
			git_error_set(GIT_ERROR_INDEXER, "incorrect packfile checksum");
			return -1;
		}

		if (parser->opts.packfile_complete) {
			int error = parser->opts.packfile_complete(
				trailer,
				hash_len,
				parser->opts.callback_data);

			if (error != 0)
				return error;
		}

		parser->state = STATE_COMPLETE;
	}

	*out = (orig_len - len);
	return 0;
}

int git_packfile_parser_parse(
	git_packfile_parser *parser,
	const void *_data,
	size_t len)
{
	const unsigned char *data = (unsigned char *)_data;

	GIT_ASSERT_ARG(parser);
	GIT_ASSERT_ARG(!len || data);

	while (len) {
		parser_state start_state = parser->state;
		size_t consumed;
		int error = 0;

		switch (parser->state) {
		case STATE_HEADER:
			error = parse_header(&consumed, parser, data, len);
			break;
		case STATE_OBJECT_HEADER_START:
		case STATE_OBJECT_HEADER:
			error = parse_object_header(&consumed, parser, data, len);
			break;
		case STATE_DELTA_HEADER:
			error = parse_delta_header(&consumed, parser, data, len);
			break;
		case STATE_OBJECT_DATA_START:
		case STATE_OBJECT_DATA:
			error = parse_object_data(&consumed, parser, data, len);
			break;
		case STATE_DELTA_DATA_START:
		case STATE_DELTA_DATA:
			error = parse_delta_data(&consumed, parser, data, len);
			break;
		case STATE_TRAILER:
			error = parse_trailer(&consumed, parser, data, len);
			break;
		case STATE_COMPLETE:
			git_error_set(GIT_ERROR_INDEXER, "packfile data after completion");
			return -1;
		default:
			GIT_ASSERT(!parser->state);
		}

		if (error == 0 && start_state < STATE_TRAILER)
			error = git_hash_update(&parser->packfile_hash, data, consumed);

		if (error != 0) {
			parser->state = STATE_FAILED;
			return error;
		}

		parser->position += consumed;
		data += consumed;
		len -= consumed;
	}

	return 0;
}

bool git_packfile_parser_complete(git_packfile_parser *parser)
{
	return (parser->state == STATE_COMPLETE);
}

void git_packfile_parser_free(git_packfile_parser *parser)
{
	if (!parser)
		return;

	git_hash_ctx_cleanup(&parser->current_hash);
	git_hash_ctx_cleanup(&parser->packfile_hash);
	git_zstream_free(&parser->zstream);
	git__free(parser);
}
