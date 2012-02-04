#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

void check(int error, const char *message)
{
	if (error) {
		fprintf(stderr, "%s (%d)\n", message, error);
		exit(1);
	}
}

int resolve_to_tree(git_repository *repo, const char *identifier, git_tree **tree)
{
	int err = 0;
	size_t len = strlen(identifier);
	git_oid oid;
	git_object *obj = NULL;

	/* try to resolve as OID */
	if (git_oid_fromstrn(&oid, identifier, len) == 0)
		git_object_lookup_prefix(&obj, repo, &oid, len, GIT_OBJ_ANY);

	/* try to resolve as reference */
	if (obj == NULL) {
		git_reference *ref, *resolved;
		if (git_reference_lookup(&ref, repo, identifier) == 0) {
			git_reference_resolve(&resolved, ref);
			git_reference_free(ref);
			if (resolved) {
				git_object_lookup(&obj, repo, git_reference_oid(resolved), GIT_OBJ_ANY);
				git_reference_free(resolved);
			}
		}
	}

	if (obj == NULL)
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

int printer(void *data, char usage, const char *line)
{
	int *last_color = data, color = 0;

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

int main(int argc, char *argv[])
{
	char path[GIT_PATH_MAX];
	git_repository *repo = NULL;
	git_tree *a, *b;
	git_diff_options opts = {0};
	git_diff_list *diff;
	char *dir = ".";
	int color = -1;

	if (argc != 3) {
		fprintf(stderr, "usage: diff <tree-oid> <tree-oid>\n");
		exit(1);
	}

	check(git_repository_discover(path, sizeof(path), dir, 0, "/"),
		"Could not discover repository");
	check(git_repository_open(&repo, path),
		"Could not open repository");

	check(resolve_to_tree(repo, argv[1], &a), "Looking up first tree");
	check(resolve_to_tree(repo, argv[2], &b), "Looking up second tree");

	check(git_diff_tree_to_tree(repo, &opts, a, b, &diff), "Generating diff");

	fputs(colors[0], stdout);

	check(git_diff_print_compact(diff, &color, printer), "Displaying diff summary");

	fprintf(stdout, "--\n");

	color = 0;

	check(git_diff_print_patch(diff, &color, printer), "Displaying diff");

	fputs(colors[0], stdout);

	git_diff_list_free(diff);
	git_tree_free(a);
	git_tree_free(b);
	git_repository_free(repo);

	return 0;
}

