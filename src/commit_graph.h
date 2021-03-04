/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_commit_graph_h__
#define INCLUDE_commit_graph_h__

#include "common.h"

#include "map.h"

/**
 * A commit-graph file.
 *
 * This file contains metadata about commits, particularly the generation
 * number for each one. This can help speed up graph operations without
 * requiring a full graph traversal.
 *
 * Support for this feature was added in git 2.19.
 */
typedef struct git_commit_graph_file {
	git_map graph_map;

	/* The OID Fanout table. */
	const uint32_t *oid_fanout;
	/* The total number of commits in the graph. */
	uint32_t num_commits;

	/* The OID Lookup table. */
	git_oid *oid_lookup;

	/*
	 * The Commit Data table. Each entry contains the OID of the commit followed
	 * by two 8-byte fields in network byte order:
	 * - The indices of the first two parents (32 bits each).
	 * - The generation number (first 30 bits) and commit time in seconds since
	 *   UNIX epoch (34 bits).
	 */
	const unsigned char *commit_data;

	/*
	 * The Extra Edge List table. Each 4-byte entry is a network byte order index
	 * of one of the i-th (i > 0) parents of commits in the `commit_data` table,
	 * when the commit has more than 2 parents.
	 */
	const unsigned char *extra_edge_list;
	/* The number of entries in the Extra Edge List table. Each entry is 4 bytes wide. */
	size_t num_extra_edge_list;

	/* The trailer of the file. Contains the SHA1-checksum of the whole file. */
	git_oid checksum;

	/* something like ".git/objects/info/commit-graph". */
	git_buf filename;
} git_commit_graph_file;

/**
 * An entry in the commit-graph file. Provides a subset of the information that
 * can be obtained from the commit header.
 */
typedef struct git_commit_graph_entry {
	/* The generation number of the commit within the graph */
	size_t generation;

	/* Time in seconds from UNIX epoch. */
	git_time_t commit_time;

	/* The number of parents of the commit. */
	size_t parent_count;

	/*
	 * The indices of the parent commits within the Commit Data table. The value
	 * of `GIT_COMMIT_GRAPH_MISSING_PARENT` indicates that no parent is in that
	 * position.
	 */
	size_t parent_indices[2];

	/* The index within the Extra Edge List of any parent after the first two. */
	size_t extra_parents_index;

	/* The SHA-1 hash of the root tree of the commit. */
	git_oid tree_oid;

	/* The SHA-1 hash of the requested commit. */
	git_oid sha1;
} git_commit_graph_entry;

int git_commit_graph_open(git_commit_graph_file **cgraph_out, const char *path);

/*
 * Returns whether the commit_graph_file needs to be reloaded since the
 * contents of the commit-graph file have changed on disk. If `path` is NULL,
 * the filename stored in `cgraph` will be used.
 */
bool git_commit_graph_needs_refresh(const git_commit_graph_file *cgraph, const char *path);

int git_commit_graph_entry_find(
		git_commit_graph_entry *e,
		const git_commit_graph_file *cgraph,
		const git_oid *short_oid,
		size_t len);
int git_commit_graph_entry_parent(
		git_commit_graph_entry *parent,
		const git_commit_graph_file *cgraph,
		const git_commit_graph_entry *entry,
		size_t n);
int git_commit_graph_close(git_commit_graph_file *cgraph);
void git_commit_graph_free(git_commit_graph_file *cgraph);

/* This is exposed for use in the fuzzers. */
int git_commit_graph_parse(git_commit_graph_file *cgraph, const unsigned char *data, size_t size);

#endif
