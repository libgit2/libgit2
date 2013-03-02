#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

static void check(int error, const char *message)
{
	if (error) {
		fprintf(stderr, "%s (%d)\n", message, error);
		exit(1);
	}
}

static int resolve_to_tree(
	git_repository *repo, const char *identifier, git_tree **tree)
{
	int err = 0;
	git_object *obj = NULL;

	if (git_revparse_single(&obj, repo, identifier) < 0)
		return GIT_ENOTFOUND;

	switch (git_object_type(obj)) {
	case GIT_OBJ_TREE:
		*tree = (git_tree *)obj;
		break;
	case GIT_OBJ_COMMIT:
		err = git_commit_tree(tree, (git_commit *)obj);
		git_object_free(obj);
		break;
	default:
		err = GIT_ENOTFOUND;
	}

	return err;
}

char *colors[] = {
	"\033[m", /* reset */
	"\033[1m", /* bold */
	"\033[31m", /* red */
	"\033[32m", /* green */
	"\033[36m" /* cyan */
};

static int printer(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char usage,
	const char *line,
	size_t line_len,
	void *data)
{
	int *last_color = data, color = 0;

	(void)delta; (void)range; (void)line_len;

	if (*last_color >= 0) {
		switch (usage) {
		case GIT_DIFF_LINE_ADDITION: color = 3; break;
		case GIT_DIFF_LINE_DELETION: color = 2; break;
		case GIT_DIFF_LINE_ADD_EOFNL: color = 3; break;
		case GIT_DIFF_LINE_DEL_EOFNL: color = 2; break;
		case GIT_DIFF_LINE_FILE_HDR: color = 1; break;
		case GIT_DIFF_LINE_HUNK_HDR: color = 4; break;
		default: color = 0;
		}
		if (color != *last_color) {
			if (*last_color == 1 || color == 1)
				fputs(colors[0], stdout);
			fputs(colors[color], stdout);
			*last_color = color;
		}
	}

	fputs(line, stdout);
	return 0;
}

static int check_uint16_param(const char *arg, const char *pattern, uint16_t *val)
{
	size_t len = strlen(pattern);
	uint16_t strval;
	char *endptr = NULL;
	if (strncmp(arg, pattern, len))
		return 0;
	strval = strtoul(arg + len, &endptr, 0);
	if (endptr == arg)
		return 0;
	*val = strval;
	return 1;
}

static int check_str_param(const char *arg, const char *pattern, const char **val)
{
	size_t len = strlen(pattern);
	if (strncmp(arg, pattern, len))
		return 0;
	*val = (const char *)(arg + len);
	return 1;
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

int main(int argc, char *argv[])
{
	git_repository *repo = NULL;
	git_tree *t1 = NULL, *t2 = NULL;
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	git_diff_list *diff;
	int i, color = -1, compact = 0, cached = 0;
	char *a, *dir = ".", *treeish1 = NULL, *treeish2 = NULL;

	/* parse arguments as copied from git-diff */

	for (i = 1; i < argc; ++i) {
		a = argv[i];

		if (a[0] != '-') {
			if (treeish1 == NULL)
				treeish1 = a;
			else if (treeish2 == NULL)
				treeish2 = a;
			else
				usage("Only one or two tree identifiers can be provided", NULL);
		}
		else if (!strcmp(a, "-p") || !strcmp(a, "-u") ||
			!strcmp(a, "--patch"))
			compact = 0;
		else if (!strcmp(a, "--cached"))
			cached = 1;
		else if (!strcmp(a, "--name-status"))
			compact = 1;
		else if (!strcmp(a, "--color"))
			color = 0;
		else if (!strcmp(a, "--no-color"))
			color = -1;
		else if (!strcmp(a, "-R"))
			opts.flags |= GIT_DIFF_REVERSE;
		else if (!strcmp(a, "-a") || !strcmp(a, "--text"))
			opts.flags |= GIT_DIFF_FORCE_TEXT;
		else if (!strcmp(a, "--ignore-space-at-eol"))
			opts.flags |= GIT_DIFF_IGNORE_WHITESPACE_EOL;
		else if (!strcmp(a, "-b") || !strcmp(a, "--ignore-space-change"))
			opts.flags |= GIT_DIFF_IGNORE_WHITESPACE_CHANGE;
		else if (!strcmp(a, "-w") || !strcmp(a, "--ignore-all-space"))
			opts.flags |= GIT_DIFF_IGNORE_WHITESPACE;
		else if (!strcmp(a, "--ignored"))
			opts.flags |= GIT_DIFF_INCLUDE_IGNORED;
		else if (!strcmp(a, "--untracked"))
			opts.flags |= GIT_DIFF_INCLUDE_UNTRACKED;
		else if (!check_uint16_param(a, "-U", &opts.context_lines) &&
			!check_uint16_param(a, "--unified=", &opts.context_lines) &&
			!check_uint16_param(a, "--inter-hunk-context=",
				&opts.interhunk_lines) &&
			!check_str_param(a, "--src-prefix=", &opts.old_prefix) &&
			!check_str_param(a, "--dst-prefix=", &opts.new_prefix))
			usage("Unknown arg", a);
	}

	/* open repo */

	check(git_repository_open_ext(&repo, dir, 0, NULL),
		"Could not open repository");

	if (treeish1)
		check(resolve_to_tree(repo, treeish1, &t1), "Looking up first tree");
	if (treeish2)
		check(resolve_to_tree(repo, treeish2, &t2), "Looking up second tree");

	/* <sha1> <sha2> */
	/* <sha1> --cached */
	/* <sha1> */
	/* --cached */
	/* nothing */

	if (t1 && t2)
		check(git_diff_tree_to_tree(&diff, repo, t1, t2, &opts), "Diff");
	else if (t1 && cached)
		check(git_diff_tree_to_index(&diff, repo, t1, NULL, &opts), "Diff");
	else if (t1) {
		git_diff_list *diff2;
		check(git_diff_tree_to_index(&diff, repo, t1, NULL, &opts), "Diff");
		check(git_diff_index_to_workdir(&diff2, repo, NULL, &opts), "Diff");
		check(git_diff_merge(diff, diff2), "Merge diffs");
		git_diff_list_free(diff2);
	}
	else if (cached) {
		check(resolve_to_tree(repo, "HEAD", &t1), "looking up HEAD");
		check(git_diff_tree_to_index(&diff, repo, t1, NULL, &opts), "Diff");
	}
	else
		check(git_diff_index_to_workdir(&diff, repo, NULL, &opts), "Diff");

	if (color >= 0)
		fputs(colors[0], stdout);

	if (compact)
		check(git_diff_print_compact(diff, printer, &color), "Displaying diff");
	else
		check(git_diff_print_patch(diff, printer, &color), "Displaying diff");

	if (color >= 0)
		fputs(colors[0], stdout);

	git_diff_list_free(diff);
	git_tree_free(t1);
	git_tree_free(t2);
	git_repository_free(repo);

	return 0;
}

