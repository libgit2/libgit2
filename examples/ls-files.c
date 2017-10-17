/*
 * libgit2 "ls-files" example - shows how to view all files currently in the index
 *
 * Written by the libgit2 contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include "common.h"

/**
 * This example demonstrates the libgit2 index APIs to roughly
 * simulate the output of `git ls-files`.
 * `git ls-files` has many options and this currently does not show them.
 * 
 * `git ls-files` base command shows all paths in the index at that time.
 * This includes staged and committed files, but unstaged files will not display.
 */

#define MAX_FILES 64

typedef struct ls_options {
	int error_unmatch;
	char *files[MAX_FILES];
	int file_count;
} ls_options;

void parse_options(ls_options *opts, int argc, char *argv[]);

int main(int argc, char *argv[]) {
	ls_options opts;
	git_repository *repo;
	git_index *index;
	const git_index_entry *entry;
	size_t entry_count;
	size_t i = 0;
	int error;

	parse_options(&opts, argc, argv);

	/* we need to initialize libgit2 */
	git_libgit2_init();

	/* we need to open the repo */
	if ((error = git_repository_open_ext(&repo, ".", 0, NULL)) != 0)
		goto cleanup;

	/* we need to load the repo's index */
	if ((error = git_repository_index(&index, repo)) != 0)
		goto cleanup;

	if (opts.error_unmatch) {
		for (i = 0; i < opts.file_count; i++) {
			const char *path = opts.files[i];
			printf("Checking first path '%s'\n", path);
			entry = git_index_get_bypath(index, path, GIT_INDEX_STAGE_NORMAL);
			if (!entry) {
				printf("Could not find path '%s'\n", path);
				return -1;
			}
		}
		goto cleanup;
	}

	/* we need to know how many entries exist in the index */
	entry_count = git_index_entrycount(index);

	/* loop through the entries by index and display their pathes */
	for (i = 0; i < entry_count; i++) {
		entry = git_index_get_byindex(index, i);
		printf("%s\n", entry->path);
	}

cleanup:
	/* free our allocated resources */
	git_index_free(index);
	git_repository_free(repo);

	/* we need to shutdown libgit2 */
	git_libgit2_shutdown();

	return error;
}

void parse_options(ls_options *opts, int argc, char *argv[]) {
	int parsing_files = 0;
	int file_idx = 0;
	struct args_info args = ARGS_INFO_INIT;
	
	memset(opts, 0, sizeof(ls_options));
	opts->error_unmatch = 0;
	opts->file_count = 0;

	if (argc < 2)
		return;

	for (args.pos = 1; args.pos < argc; ++args.pos) {
		char *a = argv[args.pos];

		if (a[0] != '-' || !strcmp(a, "--")) {
			if (parsing_files) {
				printf("%s\n", a);
				opts->files[opts->file_count++] = a;
			} else { 
				parsing_files = 1;
			}
		} else if (!strcmp(a, "--error-unmatch")) {
			opts->error_unmatch = 1;
			parsing_files = 1;
		} else {
			printf("Bad command\n");
		}
	}

	printf("file count: %d\n", opts->file_count);
	int i;
	for (i = 0; i < opts->file_count; i++) {
		printf("Path ids %d: %s\n", i, opts->files[i]);
	}
 }
