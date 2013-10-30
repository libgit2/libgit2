/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

int main (int argc, char** argv)
{
	git_index *index;
	unsigned int i, ecount;
	char *dir = ".";
	size_t dirlen;
	char out[41];
	out[40] = '\0';

	git_threads_init();

	if (argc > 2)
		fatal("usage: showindex [<repo-dir>]", NULL);
	if (argc > 1)
		dir = argv[1];

	dirlen = strlen(dir);
	if (dirlen > 5 && strcmp(dir + dirlen - 5, "index") == 0) {
		check_lg2(git_index_open(&index, dir), "could not open index", dir);
	} else {
		git_repository *repo;
		check_lg2(git_repository_open_ext(&repo, dir, 0, NULL), "could not open repository", dir);
		check_lg2(git_repository_index(&index, repo), "could not open repository index", NULL);
		git_repository_free(repo);
	}

	git_index_read(index);

	ecount = git_index_entrycount(index);
	if (!ecount)
		printf("Empty index\n");

	for (i = 0; i < ecount; ++i) {
		const git_index_entry *e = git_index_get_byindex(index, i);

		git_oid_fmt(out, &e->oid);

		printf("File Path: %s\n", e->path);
		printf("    Stage: %d\n", git_index_entry_stage(e));
		printf(" Blob SHA: %s\n", out);
		printf("File Mode: %07o\n", e->mode);
		printf("File Size: %d bytes\n", (int)e->file_size);
		printf("Dev/Inode: %d/%d\n", (int)e->dev, (int)e->ino);
		printf("  UID/GID: %d/%d\n", (int)e->uid, (int)e->gid);
		printf("    ctime: %d\n", (int)e->ctime.seconds);
		printf("    mtime: %d\n", (int)e->mtime.seconds);
		printf("\n");
	}

	git_index_free(index);
	git_threads_shutdown();

	return 0;
}
