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
 * 
 * This currently supports:
 * 	- The --error-unmatch paramter with the same output as the git cli
 *  - default ls-files behavior
 * 
 * This currently does not support:
 * 	- anything else
 * 
 */

#define MAX_FILES 64

typedef struct ls_options {
	int error_unmatch;
	char *files[MAX_FILES];
	int file_count;
} ls_options;

static void usage(const char *message, const char *arg);
void parse_options(ls_options *opts, int argc, char *argv[]);
int print_error_unmatch(ls_options *opts, git_index *index);

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

	/* if the error_unmatch flag is set, we need to print it differently */
	if (opts.error_unmatch) {
		error = print_error_unmatch(&opts, index);
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

/* Print a usage message for the program. */
static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: ls-files [--error-unmatch] [--] [<file>...]\n");
	exit(1);
}

void parse_options(ls_options *opts, int argc, char *argv[]) {
	int parsing_files = 0;
	struct args_info args = ARGS_INFO_INIT;
	
	memset(opts, 0, sizeof(ls_options));
	opts->error_unmatch = 0;
	opts->file_count = 0;

	if (argc < 2)
		return;

	for (args.pos = 1; args.pos < argc; ++args.pos) {
		char *a = argv[args.pos];

		/* if it doesn't start with a '-' or is after the '--' then it is a file */
		if (a[0] != '-' || !strcmp(a, "--")) {
			if (parsing_files) {
				opts->files[opts->file_count++] = a;
			} else { 
				parsing_files = 1;
			}
		} else if (!strcmp(a, "--error-unmatch")) {
			opts->error_unmatch = 1;
			parsing_files = 1;
		} else {
			usage("Unsupported argument", a);
		}
	}
}

int print_error_unmatch(ls_options *opts, git_index *index) {
	int i;
	const git_index_entry *entry;

	/* loop through the files found in the args and print them if they exist */
	for (i = 0; i < opts->file_count; i++) {
		const char *path = opts->files[i];

		entry = git_index_get_bypath(index, path, GIT_INDEX_STAGE_NORMAL);
		if (!entry) {
			printf("error: pathspec '%s' did not match any file(s) known to git.\n", path);
			printf("Did you forget to 'git add'?\n");
			return -1;
		}

		printf("%s\n", path);
	}
	return 0;
}
