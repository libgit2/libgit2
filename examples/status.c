/*
 * libgit2 "status" example - shows how to use the status APIs
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
 * This example demonstrates the use of the libgit2 status APIs,
 * particularly the `git_status_list` object, to roughly simulate the
 * output of running `git status`.  It serves as a simple example of
 * using those APIs to get basic status information.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Colorized or paginated output formatting
 *
 * This does have:
 *
 * - Examples of translating command line arguments to the status
 *   options settings to mimic `git status` results.
 * - A sample status formatter that matches the default "long" format
 *   from `git status`
 * - A sample status formatter that matches the "short" format
 */

enum {
	FORMAT_DEFAULT   = 0,
	FORMAT_LONG      = 1,
	FORMAT_SHORT     = 2,
	FORMAT_PORCELAIN = 3,
};

#define MAX_PATHSPEC 8

struct status_opts {
	git_status_options statusopt;
	char *repodir;
	char *pathspec[MAX_PATHSPEC];
	int npaths;
	int format;
	int zterm;
	int showbranch;
	int showsubmod;
	int repeat;
};

static void parse_opts(struct status_opts *o, int argc, char *argv[]);
static void show_branch(git_repository *repo, int format);
static void print_long(git_repository *repo, git_status_list *status);
static void print_short(git_repository *repo, git_status_list *status);
static void format_path(char **out, const char *path, git_repository *repo);
static int print_submod(git_submodule *sm, const char *name, void *payload);

int lg2_status(git_repository *repo, int argc, char *argv[])
{
	git_status_list *status;
	struct status_opts o = { GIT_STATUS_OPTIONS_INIT, "." };

	o.statusopt.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	o.statusopt.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
		GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

	parse_opts(&o, argc, argv);

	if (git_repository_is_bare(repo))
		fatal("Cannot report status on bare repository",
			git_repository_path(repo));

show_status:
	if (o.repeat)
		printf("\033[H\033[2J");

	/**
	 * Run status on the repository
	 *
	 * We use `git_status_list_new()` to generate a list of status
	 * information which lets us iterate over it at our
	 * convenience and extract the data we want to show out of
	 * each entry.
	 *
	 * You can use `git_status_foreach()` or
	 * `git_status_foreach_ext()` if you'd prefer to execute a
	 * callback for each entry. The latter gives you more control
	 * about what results are presented.
	 */
	check_lg2(git_status_list_new(&status, repo, &o.statusopt),
		"Could not get status", NULL);

	if (o.showbranch)
		show_branch(repo, o.format);

	if (o.showsubmod) {
		int submod_count = 0;
		check_lg2(git_submodule_foreach(repo, print_submod, &submod_count),
			"Cannot iterate submodules", o.repodir);
	}

	if (o.format == FORMAT_LONG)
		print_long(repo, status);
	else
		print_short(repo, status);

	git_status_list_free(status);

	if (o.repeat) {
		sleep(o.repeat);
		goto show_status;
	}

	return 0;
}

/**
 * If the user asked for the branch, let's show the short name of the
 * branch.
 */
static void show_branch(git_repository *repo, int format)
{
	int error = 0;
	const char *branch = NULL;
	git_reference *head = NULL;

	error = git_repository_head(&head, repo);

	if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND)
		branch = NULL;
	else if (!error) {
		branch = git_reference_shorthand(head);
	} else
		check_lg2(error, "failed to get current branch", NULL);

	if (format == FORMAT_LONG)
		printf("# On branch %s\n",
			branch ? branch : "Not currently on any branch.");
	else
		printf("## %s\n", branch ? branch : "HEAD (no branch)");

	git_reference_free(head);
}

static int show_change_details_long(
		int *rm_in_workdir,
		git_repository *repo,
		git_status_list *status, int listing_index_changes)
{
	int header = 0;
	int listing_workdir_changes = !listing_index_changes;
	size_t i, maxi = git_status_list_entrycount(status);
	const git_status_entry *s;
	const char *old_path, *new_path;


	for (i = 0; i < maxi; ++i) {
		char *istatus = NULL;
		char *wstatus = NULL;
		char *status_description = NULL;

		s = git_status_byindex(status, i);

		/**
		 * With `GIT_STATUS_OPT_INCLUDE_UNMODIFIED` (not used in this example)
		 * `index_to_workdir` may not be `NULL` even if there are
		 * no differences, in which case it will be a `GIT_DELTA_UNMODIFIED`.
		 */
		if (listing_workdir_changes && s->index_to_workdir == NULL)
			continue;

		if (s->status == GIT_STATUS_CURRENT)
			continue;

		if (listing_index_changes && s->status & GIT_STATUS_WT_DELETED)
			*rm_in_workdir = 1;

		if (s->status & GIT_STATUS_INDEX_NEW)
			istatus = "new file: ";
		if (s->status & GIT_STATUS_INDEX_MODIFIED)
			istatus = "modified: ";
		if (s->status & GIT_STATUS_INDEX_DELETED)
			istatus = "deleted:  ";
		if (s->status & GIT_STATUS_INDEX_RENAMED)
			istatus = "renamed:  ";
		if (s->status & GIT_STATUS_INDEX_TYPECHANGE)
			istatus = "typechange:";

		if (s->status & GIT_STATUS_WT_MODIFIED)
			wstatus = "modified: ";
		if (s->status & GIT_STATUS_WT_DELETED)
			wstatus = "deleted:  ";
		if (s->status & GIT_STATUS_WT_RENAMED)
			wstatus = "renamed:  ";
		if (s->status & GIT_STATUS_WT_TYPECHANGE)
			wstatus = "typechange:";
		if (s->status & GIT_STATUS_CONFLICTED)
			wstatus = "conflicts: ";

		if (listing_index_changes && istatus == NULL)
			continue;
		if (listing_workdir_changes && wstatus == NULL)
			continue;

		if (!header && listing_index_changes) {
			printf("# Changes to be committed:\n");
			printf("#   (use \"lg2 reset HEAD <file>...\" to unstage)\n");
			printf("#\n");
			header = 1;
		}

		if (!header && listing_workdir_changes) {
			printf("# Changes not staged for commit:\n");
			printf("#   (use \"lg2 add%s <file>...\" to update what will be committed)\n", *rm_in_workdir ? "/rm" : "");
			printf("#   (use \"lg2 checkout --force -- <file>...\" to discard changes in working directory)\n");
			printf("#\n");
			header = 1;
		}


		if (listing_index_changes) {
			old_path = s->head_to_index->old_file.path;
			new_path = s->head_to_index->new_file.path;

			status_description = istatus;
		} else {
			old_path = s->index_to_workdir->old_file.path;
			new_path = s->index_to_workdir->new_file.path;

			status_description = wstatus;
		}

		if (old_path && new_path && strcmp(old_path, new_path)) {
			char *formatted_old_path = NULL;
			char *formatted_new_path = NULL;
			format_path(&formatted_old_path, old_path, repo);
			format_path(&formatted_new_path, new_path, repo);

			printf("#\t%s  %s -> %s\n", status_description, formatted_old_path, formatted_new_path);

			free(formatted_old_path);
			free(formatted_new_path);
		} else {
			char *formatted_path = NULL;
			format_path(&formatted_path, old_path ? old_path : new_path, repo);

			printf("#\t%s  %s\n", status_description, formatted_path);

			free(formatted_path);
		}
	}

	if (header) {
		printf("#\n");
	}

	return header;
}

/**
 * This function print out an output similar to git's status command
 * in long form, including the command-line hints.
 */
static void print_long(git_repository *repo, git_status_list *status)
{
	int changed_in_index = 0;
	int changed_in_workdir = 0, rm_in_workdir = 0;
	size_t i, maxi = git_status_list_entrycount(status);
	const git_status_entry *s;
	int header;

	/** Print index changes. */
	changed_in_index = show_change_details_long(&rm_in_workdir, repo, status, 1);

	/** Print workdir changes to tracked files. */
	changed_in_workdir = show_change_details_long(&rm_in_workdir, repo, status, 0);

	/** Print untracked files. */

	header = 0;

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_WT_NEW) {
			char *formatted_path = NULL;

			if (!header) {
				printf("# Untracked files:\n");
				printf("#   (use \"lg2 add <file>...\" to include in what will be committed)\n");
				printf("#\n");
				header = 1;
			}

			format_path(&formatted_path, s->index_to_workdir->old_file.path, repo);
			printf("#\t%s\n", formatted_path);
			free(formatted_path);
		}
	}

	header = 0;

	/** Print ignored files. */

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_IGNORED) {
			char *formatted_path = NULL;

			if (!header) {
				printf("# Ignored files:\n");
				printf("#   (use \"lg2 add -f <file>...\" to include in what will be committed)\n");
				printf("#\n");
				header = 1;
			}

			format_path(&formatted_path, s->index_to_workdir->old_file.path, repo);
			printf("#\t%s\n", formatted_path);
			free(formatted_path);
		}
	}

	if (!changed_in_index && changed_in_workdir)
		printf("no changes added to commit (use \"lg2 add\" to add files to commit)\n");
}

/**
 * This version of the output prefixes each path with two status
 * columns and shows submodule status information.
 */
static void print_short(git_repository *repo, git_status_list *status)
{
	size_t i, maxi = git_status_list_entrycount(status);
	const git_status_entry *s;
	char istatus, wstatus;
	const char *extra, *a, *b, *c;
	char *fa, *fb, *fc;

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_CURRENT)
			continue;

		a = b = c = NULL;
		istatus = wstatus = ' ';
		extra = "";

		if (s->status & GIT_STATUS_INDEX_NEW)
			istatus = 'A';
		if (s->status & GIT_STATUS_INDEX_MODIFIED)
			istatus = 'M';
		if (s->status & GIT_STATUS_INDEX_DELETED)
			istatus = 'D';
		if (s->status & GIT_STATUS_INDEX_RENAMED)
			istatus = 'R';
		if (s->status & GIT_STATUS_INDEX_TYPECHANGE)
			istatus = 'T';

		if (s->status & GIT_STATUS_WT_NEW) {
			if (istatus == ' ')
				istatus = '?';
			wstatus = '?';
		}
		if (s->status & GIT_STATUS_WT_MODIFIED)
			wstatus = 'M';
		if (s->status & GIT_STATUS_WT_DELETED)
			wstatus = 'D';
		if (s->status & GIT_STATUS_WT_RENAMED)
			wstatus = 'R';
		if (s->status & GIT_STATUS_WT_TYPECHANGE)
			wstatus = 'T';

		if (s->status & GIT_STATUS_IGNORED) {
			istatus = '!';
			wstatus = '!';
		}

		if (istatus == '?' && wstatus == '?')
			continue;

		/**
		 * A commit in a tree is how submodules are stored, so
		 * let's go take a look at its status.
		 */
		if (s->index_to_workdir &&
			s->index_to_workdir->new_file.mode == GIT_FILEMODE_COMMIT)
		{
			unsigned int smstatus = 0;

			if (!git_submodule_status(&smstatus, repo, s->index_to_workdir->new_file.path,
						  GIT_SUBMODULE_IGNORE_UNSPECIFIED)) {
				if (smstatus & GIT_SUBMODULE_STATUS_WD_MODIFIED)
					extra = " (new commits)";
				else if (smstatus & GIT_SUBMODULE_STATUS_WD_INDEX_MODIFIED)
					extra = " (modified content)";
				else if (smstatus & GIT_SUBMODULE_STATUS_WD_WD_MODIFIED)
					extra = " (modified content)";
				else if (smstatus & GIT_SUBMODULE_STATUS_WD_UNTRACKED)
					extra = " (untracked content)";
			}
		}

		/**
		 * Now that we have all the information, format the output.
		 */

		if (s->head_to_index) {
			a = s->head_to_index->old_file.path;
			b = s->head_to_index->new_file.path;
		}
		if (s->index_to_workdir) {
			if (!a)
				a = s->index_to_workdir->old_file.path;
			if (!b)
				b = s->index_to_workdir->old_file.path;
			c = s->index_to_workdir->new_file.path;
		}

		format_path(&fa, a, repo);
		format_path(&fb, b, repo);
		format_path(&fc, c, repo);

		if (istatus == 'R') {
			if (wstatus == 'R')
				printf("%c%c %s %s %s%s\n", istatus, wstatus, a, b, c, extra);
			else
				printf("%c%c %s %s%s\n", istatus, wstatus, a, b, extra);
		} else {
			if (wstatus == 'R')
				printf("%c%c %s %s%s\n", istatus, wstatus, a, c, extra);
			else
				printf("%c%c %s%s\n", istatus, wstatus, a, extra);
		}

		free(fa);
		free(fb);
		free(fc);
	}

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_WT_NEW) {
			char *formatted_path = NULL;
			format_path(&formatted_path, s->index_to_workdir->old_file.path, repo);

			printf("?? %s\n", formatted_path);

			free(formatted_path);
		}
	}
}

static int print_submod(git_submodule *sm, const char *name, void *payload)
{
	git_repository *repo = git_submodule_owner(sm);
	int *count = payload;
	char *path;
	(void)name;

	if (*count == 0)
		printf("# Submodules\n");
	(*count)++;

	format_path(&path, git_submodule_path(sm), repo);
	printf("# - submodule '%s' at %s\n", git_submodule_name(sm), path);
	free(path);

	return 0;
}

static void format_path(char **out, const char *path, git_repository *repo)
{
	*out = NULL;

	if (path != NULL) {
		get_relpath_to(out, path, repo);
	}
}

static void usage_error(const char *program_name, const char *bad_arg)
{
	fprintf(stderr, "Unrecognised argument: %s\n", bad_arg);
	fprintf(stderr,
		"USAGE: %s [-s|-b|-z] [--short|--long]\n"
		"          [--porcelain] [--branch] [--ignored]\n"
		"          [--untracked-files=<no|normal|all>]\n"
		"          [--repeat] [--list-submodules]\n"
		"Warning: Some of the above options (e.g. --porcelain)\n"
		"         are not fully implemented.\n"
		, program_name);
	exit(1);
}

/**
 * Parse options that git's status command supports.
 */
static void parse_opts(struct status_opts *o, int argc, char *argv[])
{
	struct args_info args = ARGS_INFO_INIT;

	for (args.pos = 1; args.pos < argc; ++args.pos) {
		char *a = argv[args.pos];

		if (a[0] != '-') {
			if (o->npaths < MAX_PATHSPEC)
				o->pathspec[o->npaths++] = a;
			else
				fatal("Example only supports a limited pathspec", NULL);
		}
		else if (!strcmp(a, "-s") || !strcmp(a, "--short"))
			o->format = FORMAT_SHORT;
		else if (!strcmp(a, "--long"))
			o->format = FORMAT_LONG;
		else if (!strcmp(a, "--porcelain"))
			o->format = FORMAT_PORCELAIN;
		else if (!strcmp(a, "-b") || !strcmp(a, "--branch"))
			o->showbranch = 1;
		else if (!strcmp(a, "-z")) {
			o->zterm = 1;
			if (o->format == FORMAT_DEFAULT)
				o->format = FORMAT_PORCELAIN;
		}
		else if (!strcmp(a, "--ignored"))
			o->statusopt.flags |= GIT_STATUS_OPT_INCLUDE_IGNORED;
		else if (!strcmp(a, "-uno") ||
				 !strcmp(a, "--untracked-files=no"))
			o->statusopt.flags &= ~GIT_STATUS_OPT_INCLUDE_UNTRACKED;
		else if (!strcmp(a, "-unormal") ||
				 !strcmp(a, "--untracked-files=normal"))
			o->statusopt.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;
		else if (!strcmp(a, "-uall") ||
				 !strcmp(a, "--untracked-files=all"))
			o->statusopt.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED |
				GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
		else if (!strcmp(a, "--ignore-submodules=all"))
			o->statusopt.flags |= GIT_STATUS_OPT_EXCLUDE_SUBMODULES;
		else if (!strncmp(a, "--git-dir=", strlen("--git-dir=")))
			o->repodir = a + strlen("--git-dir=");
		else if (!strcmp(a, "--repeat"))
			o->repeat = 10;
		else if (match_int_arg(&o->repeat, &args, "--repeat", 0))
			/* okay */;
		else if (!strcmp(a, "--list-submodules"))
			o->showsubmod = 1;
		else
			usage_error(a, argv[0]);
	}

	if (o->format == FORMAT_DEFAULT)
		o->format = FORMAT_LONG;
	if (o->format == FORMAT_LONG)
		o->showbranch = 1;
	if (o->npaths > 0) {
		o->statusopt.pathspec.strings = o->pathspec;
		o->statusopt.pathspec.count   = o->npaths;
	}
}
