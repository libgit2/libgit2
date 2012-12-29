#include <git2.h>
#include <stdio.h>
#include <string.h>

int main (int argc, char** argv)
{
	git_repository *repo = NULL;
	git_index *index;
	unsigned int i, ecount;
	char *dir = ".";
	size_t dirlen;
	char out[41];
	out[40] = '\0';

	if (argc > 1)
		dir = argv[1];
	if (!dir || argc > 2) {
		fprintf(stderr, "usage: showindex [<repo-dir>]\n");
		return 1;
	}

	dirlen = strlen(dir);
	if (dirlen > 5 && strcmp(dir + dirlen - 5, "index") == 0) {
		if (git_index_open(&index, dir) < 0) {
			fprintf(stderr, "could not open index: %s\n", dir);
			return 1;
		}
	} else {
		if (git_repository_open_ext(&repo, dir, 0, NULL) < 0) {
			fprintf(stderr, "could not open repository: %s\n", dir);
			return 1;
		}
		if (git_repository_index(&index, repo) < 0) {
			fprintf(stderr, "could not open repository index\n");
			return 1;
		}
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
	git_repository_free(repo);

	return 0;
}

