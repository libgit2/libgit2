/*
 * libgit2 index (dircache) fuzz target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

/*
 * Fuzzes the git index binary format parser and the staging APIs built
 * on top of it.
 *
 * The .git/index file of a repository is attacker-controlled data
 * whenever an application inspects a repository it did not create (one
 * cloned from a hostile source, unpacked from an archive or checked out
 * from a bundle): the file is parsed on open and drives entry parsing
 * (versions 2/3/4, incl. prefix-compressed paths), extension parsing
 * (TREE, REUC, ...) and footer checksum handling.
 *
 * The harness treats the fuzzed bytes as the index file of a shared
 * repository fixture and exercises:
 *  - git_index_open + git_index_read: the binary parser itself
 *  - entry iteration (git_index_entrycount / git_index_get_byindex)
 *  - conflict (staged entries / resolve-undo) iteration
 *  - the encode direction: git_index_write (v4 and v2 serialisation)
 *  - tree building from attacker-controlled entries into the object
 *    database: git_index_write_tree_to + git_tree_lookup
 *
 * Objects written during tree building are removed after each iteration
 * so the shared fixture stays fixed-size across a campaign.
 */

#include <stdio.h>
#include <string.h>

#include "git2.h"
#include "futils.h"

#include "standalone_driver.h"
#include "fuzzer_utils.h"

#define UNUSED(x) (void)(x)

/* Cap the amount of attacker bytes handed to the parser so that memory
 * and CPU use stay attributable to upstream's parsing choices, not to
 * raw input volume. */
#define MAX_INDEX_SIZE (1024 * 1024)

static git_repository *repo;
static char objects_path[GIT_PATH_MAX];
static char index_path[GIT_PATH_MAX];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	const char *gitdir, *sep;
	size_t len;

	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();

	repo = fuzzer_repo_init();

	gitdir = git_repository_path(repo);
	if (gitdir == NULL)
		abort();

	len = strlen(gitdir);
	sep = (len > 0 && gitdir[len - 1] == '/') ? "" : "/";

	if (snprintf(objects_path, sizeof(objects_path),
		     "%s%sobjects", gitdir, sep) < 0 ||
	    snprintf(index_path, sizeof(index_path),
		     "%s%sindex", gitdir, sep) < 0)
		abort();

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_index_conflict_iterator *conflict_it = NULL;
	const git_index_entry *ancestor, *ours, *theirs;
	git_oid tree_id;
	FILE *fp;
	size_t i, entry_count;

	if (size > MAX_INDEX_SIZE)
		return 0;

	/* Make the attacker bytes the repository's index file. */
	fp = fopen(index_path, "wb");
	if (fp == NULL)
		return 0;
	if (size > 0 && fwrite(data, 1, size, fp) != size) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	/* Parse the index: header, entries (v2/v3/v4), extensions, checksum. */
	if (git_index_open(&index, index_path) < 0)
		goto done;
	if (git_index_read(index, 1) < 0)
		goto done;

	/* Walk every parsed entry. */
	entry_count = git_index_entrycount(index);
	for (i = 0; i < entry_count; i++) {
		if (git_index_get_byindex(index, i) == NULL)
			break;
	}

	/* Walk any conflict (staged entries / resolve-undo) state. */
	if (git_index_conflict_iterator_new(&conflict_it, index) >= 0) {
		while (git_index_conflict_next(&ancestor, &ours,
					       &theirs, conflict_it) >= 0)
			; /* Void. */
		git_index_conflict_iterator_free(conflict_it);
	}

	/* Encode direction: re-serialise as v4 (path compression) and v2. */
	if (git_index_set_version(index, 4) == 0)
		git_index_write(index);
	if (git_index_set_version(index, 2) == 0)
		git_index_write(index);

	/* Build a tree from the attacker-controlled entries into the ODB. */
	if (git_index_write_tree_to(&tree_id, index, repo) >= 0)
		git_tree_lookup(&tree, repo, &tree_id);

done:
	git_tree_free(tree);
	git_index_free(index);

	/*
	 * Remove the objects written above so the fixture stays
	 * fixed-size, but keep the objects directory itself in place:
	 * removing it entirely would make the repository's lazily-created
	 * object database hit its missing-directory error path on later
	 * iterations, which no legitimate caller pattern produces.
	 */
	git_futils_rmdir_r(objects_path, NULL, GIT_RMDIR_REMOVE_FILES);
	git_futils_mkdir_r(objects_path, 0777);

	return 0;
}
