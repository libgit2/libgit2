/*
 * libgit2 "add" example - shows how to modify the index
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
 * The following example demonstrates how to add files with libgit2.
 *
 * It will use the repository in the current working directory, and act
 * on files passed as its parameters.
 *
 * Recognized options are:
 *   -v/--verbose: show the file's status after acting on it.
 *   -n/--dry-run: do not actually change the index.
 *   -u/--update: update the index instead of adding to it.
 */

enum index_mode {
	INDEX_NONE,
	INDEX_ADD,
	INDEX_REMOVE
};

struct index_options {
	int dry_run;
	int verbose;
	git_repository *repo;
	enum index_mode mode;
	int force;
	int some_ignored;

	// Specific to INDEX_ADD
	int add_update;

	// Specific to INDEX_REMOVE
	int update_index_only;
};

/* Forward declarations for helpers */
static void parse_opts(const char **repo_path, struct index_options *opts, struct args_info *args);
static void init_array(git_strarray *array, git_repository *repo, struct args_info *args);
static int print_matched_cb(const char *path, const char *matched_pathspec, void *payload);
static int filter_matched_cb(const char *path, const char *matched_pathspec, void *payload);

int lg2_add(git_repository *repo, int argc, char **argv)
{
	git_index_matched_path_cb matched_cb = NULL;
	git_index *index;
	git_strarray array = {0};
	struct index_options options = {0};
	struct args_info args = ARGS_INFO_INIT;

	options.mode = INDEX_ADD;

	/* Parse the options & arguments. */
	parse_opts(NULL, &options, &args);
	init_array(&array, repo, &args);

	/* Grab the repository's index. */
	check_lg2(git_repository_index(&index, repo), "Could not open repository index", NULL);

	/**
	 * We'll filter matched paths based on the `.gitignore`.
	 * Note that while `git_index_add_all` supports a flag that
	 * determines whether the `.gitignore` is checked, but `git_index_update_all`
	 * currently doesn't.
	 */
	matched_cb = &filter_matched_cb;

	options.repo = repo;

	/* Perform the requested action with the index and files */
	if (options.mode == INDEX_REMOVE) {
		// Remove everything in &array.
		git_index_remove_all(index, &array, matched_cb, &options);

		if (!options.update_index_only) {
			size_t i;
			fprintf(stderr,
				"Warning: Currently `lg2 rm file1 file2...` is not implemented.\n"
				"	While the given files have been removed from the index,"
				" they have not been deleted.\n\n");
			fprintf(stderr, "Running `lg2 rm --cached files...` instead will"
							" hide this warning.\n\n");
			fprintf(stderr,
				"Please manually delete the following files:\n");

			for (i = 0; i < array.count; i++) {
				fprintf(stderr, "\t%s\n", array.strings[i]);
			}

			fprintf(stderr, "On most POSIX systems, this can be done with:\n");
			fprintf(stderr, "\t rm -rf ");

			for (i = 0; i < array.count; i++) {
				fprintf(stderr, " %s", array.strings[i]);
			}

			fprintf(stderr, "\n\n");
		}
	} else if (options.add_update) {
		if (options.force) {
			fprintf(stderr, "Warning: --force is ignored when using the -u option.\n");
			fprintf(stderr,
				"To remove files from the index, use"
				"`lg2 rm --cached file1 file2...` \n");
		}

		git_index_update_all(index, &array, matched_cb, &options);
	} else {
		unsigned int add_options = GIT_INDEX_ADD_FORCE;

		/**
		 * If we were only using `git_index_add_all`,
		 * we could set `add_options` to `GIT_INDEX_ADD_FORCE` if we decided
		 * not to check the `.gitignore`.
		 *
		 * In this case, `GIT_INDEX_ADD_FORCE` is enabled because we want our
		 * callback to decide whether to force-add files.
		 */

		git_index_add_all(index, &array, add_options, matched_cb, &options);
	}

	if (options.some_ignored) {
		printf(
			"Warning: Some paths were ignored as per one of your .gitignore files.\n"
			"Re-run with --verbose to see which files were ignored.\n"
			"Re-run with --force to forcibly add these files.\n");
	}

	/* Cleanup memory */
	git_index_write(index);
	git_index_free(index);
	git_strarray_dispose(&array);

	return 0;
}

/*
 * This callback is called for each file under consideration by
 * git_index_(update|add)_all above.
 * It makes uses of the callback's ability to abort the action.
 */
int print_matched_cb(const char *path, const char *matched_pathspec, void *payload)
{
	struct index_options *opts = (struct index_options *)(payload);
	int ret;
	unsigned status;
	(void)matched_pathspec;

	if (opts->mode == INDEX_REMOVE) {
		printf("remove '%s'\n", path);

		return opts->dry_run ? 1 : 0;
	}

	/* Get the file status */
	if (git_status_file(&status, opts->repo, path) < 0)
		return -1;

	if ((status & GIT_STATUS_WT_MODIFIED) || (status & GIT_STATUS_WT_NEW)) {
		printf("add '%s'\n", path);
		ret = 0;
	} else {
		ret = 1;
	}

	if (opts->dry_run)
		ret = 1;

	return ret;
}

static int filter_matched_cb(const char *path, const char *matched_pathspec, void *payload) {
	int result = 0, err = 0;
	struct index_options *options = (struct index_options *)(payload);

	UNUSED(matched_pathspec);

	// Check the .gitignore
	if (!options->force) {
		int ignored = 0;

		err = git_ignore_path_is_ignored(&ignored, options->repo, path);
		if (err) {
			// If err is negative, we stop processing. Return the (negative) error.
			fprintf(stderr, "Unable to process .gitignore file!\n");
			return err;
		}

		// If ignored, result becomes 1, so exclude the path.
		// Else, ignored is zero, so we include the path.
		result = ignored;
	}

	// If excluding the path, stop now.
	if (result != 0) {
		if (options->verbose) {
			fprintf(stderr, "Ignoring %s: is in the .gitignore.\n", path);
		}

		options->some_ignored = 1;
		return result;
	}

	if (options->verbose == 1 || options->dry_run == 1) {
		result = print_matched_cb(path, matched_pathspec, payload);
	}

	/**
	 * Here, if we return 0, the file is added. If positive, the file is skipped.
	 */
	return result;
}

static void init_array(git_strarray *array, git_repository *repo, struct args_info *args)
{
	size_t i;

	array->count = args->argc - args->pos;
	array->strings = calloc(array->count, sizeof(char *));
	assert(array->strings != NULL);

	for (i = 0; i < array->count; i++) {
		get_repopath_to(&array->strings[i], args->argv[i + args->pos], repo);
	}

	return;
}

void print_usage(void)
{
	fprintf(stderr, "usage: lg2 add [options] [--] file-spec [file-spec] [...]\n\n");
	fprintf(stderr, "\t-n, --dry-run    dry run\n");
	fprintf(stderr, "\t-v, --verbose    be verbose\n");
	fprintf(stderr, "\t-u, --update     update tracked files\n");
	fprintf(stderr, "\t-f, --force      add files, even if in .gitignore\n\n\n");

	fprintf(stderr, "usage: lg2 rm [options] [--] file-spec [file-spec] [...]\n\n");
	fprintf(stderr, "\t-n, --dry-run    dry run\n");
	fprintf(stderr, "\t-v, --verbose    be verbose\n");
	fprintf(stderr, "\t--cached         only update the index (not the working tree)\n");
	fprintf(stderr, "\t-f, --force      remove files, even if in .gitignore\n");
	fprintf(stderr, "Note: At present, `lg2 rm` always behaves as if it were given "
			"--cached.\n");

	exit(1);
}

static void parse_opts(const char **repo_path, struct index_options *opts, struct args_info *args)
{
	if (args->argc <= 1)
		print_usage();

	for (args->pos = 0; args->pos < args->argc; ++args->pos) {
		const char *curr = args->argv[args->pos];

		if (curr[0] != '-') {
			if (!strcmp("add", curr) && args->pos < 1) {
				opts->mode = INDEX_ADD;
				continue;
			} else if (!strcmp("rm", curr) && args->pos < 1) {
				opts->mode = INDEX_REMOVE;
				continue;
			} else if (opts->mode == INDEX_NONE) {
				fprintf(stderr, "missing command: %s", curr);
				print_usage();
				break;
			} else {
				/* We might be looking at a filename */
				break;
			}
		} else if (match_bool_arg(&opts->verbose, args, "--verbose") ||
				   match_bool_arg(&opts->verbose, args, "-v") ||
				   match_bool_arg(&opts->dry_run, args, "--dry-run") ||
				   match_str_arg(repo_path, args, "--git-dir")) {
			continue;
		} else if (opts->mode == INDEX_REMOVE && !strcmp("--cached", curr)) {
			opts->update_index_only = 1;
		} else if (opts->mode == INDEX_ADD &&
				  (!strcmp("-u", curr) || !strcmp("--update", curr))) {
			opts->add_update = 1;
		} else if (!strcmp("--help", curr) || !strcmp("-h", curr)) {
			print_usage();
			break;
		} else if (!strcmp("--force", curr) || !strcmp("-f", curr)) {
			opts->force = 1;
		} else if (match_arg_separator(args)) {
			break;
		} else {
			fprintf(stderr, "Unsupported option %s.\n", curr);
			print_usage();
		}
	}

	// match_bool_arg sets its result to -1 if neither --no-[arg here] nor
	// --[arg-here] is present.
	if (opts->dry_run != 1) opts->dry_run = 0;
	if (opts->verbose != 1) opts->verbose = 0;
}

