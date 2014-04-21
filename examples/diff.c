/*
 * libgit2 "diff" example - shows how to use the diff API
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
 * This example demonstrates the use of the libgit2 diff APIs to
 * create `git_diff` objects and display them, emulating a number of
 * core Git `diff` command line options.
 *
 * This covers on a portion of the core Git diff options and doesn't
 * have particularly good error handling, but it should show most of
 * the core libgit2 diff APIs, including various types of diffs and
 * how to do renaming detection and patch formatting.
 */

static const char *colors[] = {
	"\033[m", /* reset */
	"\033[1m", /* bold */
	"\033[31m", /* red */
	"\033[32m", /* green */
	"\033[36m" /* cyan */
};

enum {
	OUTPUT_DIFF = 0,
	OUTPUT_STAT = 1,
	OUTPUT_SHORTSTAT = 2,
	OUTPUT_NUMSTAT = 3
};

enum {
	CACHE_NORMAL = 0,
	CACHE_ONLY = 1,
	CACHE_NONE = 2
};

/** The 'opts' struct captures all the various parsed command line options. */
struct opts {
	git_diff_options diffopts;
	git_diff_find_options findopts;
	int color;
	int cache;
	int output;
	git_diff_format_t format;
	const char *treeish1;
	const char *treeish2;
	const char *dir;
};

/** These functions are implemented at the end */
static void usage(const char *message, const char *arg);
static void parse_opts(struct opts *o, int argc, char *argv[]);
static int color_printer(
	const git_diff_delta*, const git_diff_hunk*, const git_diff_line*, void*);
static void diff_print_stats(git_diff *diff, struct opts *o);

int main(int argc, char *argv[])
{
	git_repository *repo = NULL;
	git_tree *t1 = NULL, *t2 = NULL;
	git_diff *diff;
	struct opts o = {
		GIT_DIFF_OPTIONS_INIT, GIT_DIFF_FIND_OPTIONS_INIT,
		-1, 0, 0, GIT_DIFF_FORMAT_PATCH, NULL, NULL, "."
	};

	git_threads_init();

	parse_opts(&o, argc, argv);

	check_lg2(git_repository_open_ext(&repo, o.dir, 0, NULL),
		"Could not open repository", o.dir);

	/**
	 * Possible argument patterns:
	 *
	 *  * &lt;sha1&gt; &lt;sha2&gt;
	 *  * &lt;sha1&gt; --cached
	 *  * &lt;sha1&gt;
	 *  * --cached
	 *  * --nocache (don't use index data in diff at all)
	 *  * nothing
	 *
	 * Currently ranged arguments like &lt;sha1&gt;..&lt;sha2&gt; and &lt;sha1&gt;...&lt;sha2&gt;
	 * are not supported in this example
	 */

	if (o.treeish1)
		treeish_to_tree(&t1, repo, o.treeish1);
	if (o.treeish2)
		treeish_to_tree(&t2, repo, o.treeish2);

	if (t1 && t2)
		check_lg2(
			git_diff_tree_to_tree(&diff, repo, t1, t2, &o.diffopts),
			"diff trees", NULL);
	else if (o.cache != CACHE_NORMAL) {
		if (!t1)
			treeish_to_tree(&t1, repo, "HEAD");

		if (o.cache == CACHE_NONE)
			check_lg2(
				git_diff_tree_to_workdir(&diff, repo, t1, &o.diffopts),
				"diff tree to working directory", NULL);
		else
			check_lg2(
				git_diff_tree_to_index(&diff, repo, t1, NULL, &o.diffopts),
				"diff tree to index", NULL);
	}
	else if (t1)
		check_lg2(
			git_diff_tree_to_workdir_with_index(&diff, repo, t1, &o.diffopts),
			"diff tree to working directory", NULL);
	else
		check_lg2(
			git_diff_index_to_workdir(&diff, repo, NULL, &o.diffopts),
			"diff index to working directory", NULL);

	/** Apply rename and copy detection if requested. */

	if ((o.findopts.flags & GIT_DIFF_FIND_ALL) != 0)
		check_lg2(
			git_diff_find_similar(diff, &o.findopts),
			"finding renames and copies", NULL);

	/** Generate simple output using libgit2 display helper. */

	switch (o.output) {
	case OUTPUT_STAT:
	case OUTPUT_NUMSTAT:
	case OUTPUT_SHORTSTAT:
		diff_print_stats(diff, &o);
		break;

	case OUTPUT_DIFF:
		if (o.color >= 0)
			fputs(colors[0], stdout);

		check_lg2(
			git_diff_print(diff, o.format, color_printer, &o.color),
			"displaying diff", NULL);

		if (o.color >= 0)
			fputs(colors[0], stdout);
		break;

	default:
		usage("Unknown output format", "programmer error");
	}

	/** Cleanup before exiting. */

	git_diff_free(diff);
	git_tree_free(t1);
	git_tree_free(t2);
	git_repository_free(repo);

	git_threads_shutdown();

	return 0;
}

static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: diff [<tree-oid> [<tree-oid>]]\n");
	exit(1);
}

/** This implements very rudimentary colorized output. */
static int color_printer(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *data)
{
	int *last_color = data, color = 0;

	(void)delta; (void)hunk;

	if (*last_color >= 0) {
		switch (line->origin) {
		case GIT_DIFF_LINE_ADDITION:  color = 3; break;
		case GIT_DIFF_LINE_DELETION:  color = 2; break;
		case GIT_DIFF_LINE_ADD_EOFNL: color = 3; break;
		case GIT_DIFF_LINE_DEL_EOFNL: color = 2; break;
		case GIT_DIFF_LINE_FILE_HDR:  color = 1; break;
		case GIT_DIFF_LINE_HUNK_HDR:  color = 4; break;
		default: break;
		}

		if (color != *last_color) {
			if (*last_color == 1 || color == 1)
				fputs(colors[0], stdout);
			fputs(colors[color], stdout);
			*last_color = color;
		}
	}

	return diff_output(delta, hunk, line, stdout);
}

/** Parse arguments as copied from git-diff. */
static void parse_opts(struct opts *o, int argc, char *argv[])
{
	struct args_info args = ARGS_INFO_INIT;


	for (args.pos = 1; args.pos < argc; ++args.pos) {
		const char *a = argv[args.pos];

		if (a[0] != '-') {
			if (o->treeish1 == NULL)
				o->treeish1 = a;
			else if (o->treeish2 == NULL)
				o->treeish2 = a;
			else
				usage("Only one or two tree identifiers can be provided", NULL);
		}
		else if (!strcmp(a, "-p") || !strcmp(a, "-u") ||
			!strcmp(a, "--patch"))
			o->format = GIT_DIFF_FORMAT_PATCH;
		else if (!strcmp(a, "--cached"))
			o->cache = CACHE_ONLY;
		else if (!strcmp(a, "--nocache"))
			o->cache = CACHE_NONE;
		else if (!strcmp(a, "--name-only") || !strcmp(a, "--format=name"))
			o->format = GIT_DIFF_FORMAT_NAME_ONLY;
		else if (!strcmp(a, "--name-status") ||
				!strcmp(a, "--format=name-status"))
			o->format = GIT_DIFF_FORMAT_NAME_STATUS;
		else if (!strcmp(a, "--raw") || !strcmp(a, "--format=raw"))
			o->format = GIT_DIFF_FORMAT_RAW;
		else if (!strcmp(a, "--format=diff-index")) {
			o->format = GIT_DIFF_FORMAT_RAW;
			o->diffopts.id_abbrev = 40;
		}
		else if (!strcmp(a, "--color"))
			o->color = 0;
		else if (!strcmp(a, "--no-color"))
			o->color = -1;
		else if (!strcmp(a, "-R"))
			o->diffopts.flags |= GIT_DIFF_REVERSE;
		else if (!strcmp(a, "-a") || !strcmp(a, "--text"))
			o->diffopts.flags |= GIT_DIFF_FORCE_TEXT;
		else if (!strcmp(a, "--ignore-space-at-eol"))
			o->diffopts.flags |= GIT_DIFF_IGNORE_WHITESPACE_EOL;
		else if (!strcmp(a, "-b") || !strcmp(a, "--ignore-space-change"))
			o->diffopts.flags |= GIT_DIFF_IGNORE_WHITESPACE_CHANGE;
		else if (!strcmp(a, "-w") || !strcmp(a, "--ignore-all-space"))
			o->diffopts.flags |= GIT_DIFF_IGNORE_WHITESPACE;
		else if (!strcmp(a, "--ignored"))
			o->diffopts.flags |= GIT_DIFF_INCLUDE_IGNORED;
		else if (!strcmp(a, "--untracked"))
			o->diffopts.flags |= GIT_DIFF_INCLUDE_UNTRACKED;
		else if (!strcmp(a, "--patience"))
			o->diffopts.flags |= GIT_DIFF_PATIENCE;
		else if (!strcmp(a, "--minimal"))
			o->diffopts.flags |= GIT_DIFF_MINIMAL;
		else if (!strcmp(a, "--stat"))
			o->output = OUTPUT_STAT;
		else if (!strcmp(a, "--numstat"))
			o->output = OUTPUT_NUMSTAT;
		else if (!strcmp(a, "--shortstat"))
			o->output = OUTPUT_SHORTSTAT;
		else if (match_uint16_arg(
				&o->findopts.rename_threshold, &args, "-M") ||
			match_uint16_arg(
				&o->findopts.rename_threshold, &args, "--find-renames"))
			o->findopts.flags |= GIT_DIFF_FIND_RENAMES;
		else if (match_uint16_arg(
				&o->findopts.copy_threshold, &args, "-C") ||
			match_uint16_arg(
				&o->findopts.copy_threshold, &args, "--find-copies"))
			o->findopts.flags |= GIT_DIFF_FIND_COPIES;
		else if (!strcmp(a, "--find-copies-harder"))
			o->findopts.flags |= GIT_DIFF_FIND_COPIES_FROM_UNMODIFIED;
		else if (is_prefixed(a, "-B") || is_prefixed(a, "--break-rewrites"))
			/* TODO: parse thresholds */
			o->findopts.flags |= GIT_DIFF_FIND_REWRITES;
		else if (!match_uint16_arg(
				&o->diffopts.context_lines, &args, "-U") &&
			!match_uint16_arg(
				&o->diffopts.context_lines, &args, "--unified") &&
			!match_uint16_arg(
				&o->diffopts.interhunk_lines, &args, "--inter-hunk-context") &&
			!match_uint16_arg(
				&o->diffopts.id_abbrev, &args, "--abbrev") &&
			!match_str_arg(&o->diffopts.old_prefix, &args, "--src-prefix") &&
			!match_str_arg(&o->diffopts.new_prefix, &args, "--dst-prefix") &&
			!match_str_arg(&o->dir, &args, "--git-dir"))
			usage("Unknown command line argument", a);
	}
}

/** Display diff output with "--numstat" or "--shortstat" */
static void diff_print_stats(git_diff *diff, struct opts *o)
{
	git_patch *patch;
	size_t d, ndeltas = git_diff_num_deltas(diff);
	size_t nadditions, ndeletions;
	long nadditions_sum, ndeletions_sum;

	nadditions_sum = 0;
	ndeletions_sum = 0;

	for (d = 0; d < ndeltas; d++){
		check_lg2(
			git_patch_from_diff(&patch, diff, d),
			"generating patch from diff", NULL);

		check_lg2(
			git_patch_line_stats(NULL, &nadditions, &ndeletions, patch),
			"generating the number of additions and deletions", NULL);

		if (o->output == OUTPUT_NUMSTAT) {
			const git_diff_delta *delta = git_patch_get_delta(patch);
			printf("%ld\t%ld\t%s\n",
				   (long)nadditions, (long)ndeletions, delta->new_file.path);
		}
		else if (o->output == OUTPUT_STAT) {
			const git_diff_delta *delta = git_patch_get_delta(patch);
			printf(" %s\t| %ld\t(%ld+ %ld-)\n",
				   delta->new_file.path, (long)nadditions + (long)ndeletions,
				   (long)nadditions, (long)ndeletions);
		}

		nadditions_sum += nadditions;
		ndeletions_sum += ndeletions;

		git_patch_free(patch);
	}

	if (o->output != OUTPUT_NUMSTAT && ndeltas > 0) {
	    printf(" %ld %s", (long)ndeltas,
			1 == ndeltas ? "file changed" : "files changed");

	    if (nadditions_sum) {
			printf(", %ld ",nadditions_sum);
			printf("%s", 1 == nadditions_sum ? "insertion(+)" : "insertions(+)");
	    }

	    if (ndeletions_sum) {
			printf(", %ld ",ndeletions_sum);
			printf("%s", 1 == ndeletions_sum ? "deletion(-)" : "deletions(-)");
	    }

	    printf("\n");
	}
}

