#include <git2.h>
#include <stdio.h>

int main (int argc, char** argv)
{
	git_repository *repo;
	git_index *index;
	unsigned int i, ecount;
	char *dir = ".";
	char out[41];
	out[40] = '\0';

	if (argc > 1)
		dir = argv[1];
	if (argc > 2) {
		fprintf(stderr, "usage: showindex [<repo-dir>]\n");
		return 1;
	}

	if (git_repository_open_ext(&repo, dir, 0, NULL) < 0) {
		fprintf(stderr, "could not open repository: %s\n", dir);
		return 1;
	}

	git_repository_index(&index, repo);
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
		printf("File Size: %d\n", (int)e->file_size);
		printf("   Device: %d\n", (int)e->dev);
		printf("    Inode: %d\n", (int)e->ino);
		printf("      UID: %d\n", (int)e->uid);
		printf("      GID: %d\n", (int)e->gid);
		printf("    ctime: %d\n", (int)e->ctime.seconds);
		printf("    mtime: %d\n", (int)e->mtime.seconds);
		printf("\n");
	}

	git_index_free(index);
	git_repository_free(repo);

	return 0;
}

