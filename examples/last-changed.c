/*
 * libgit2 "last-changed" example - get last commit modifying a file
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

static void usage(void)
{
	fprintf(stderr, "usage: last-changed [--git-dir=DIR] pathname ...\n");
	exit(1);
}

static int mark_pathspec_match(
	const git_diff *, const git_diff_delta *, const char *, void *);

typedef struct {
	git_diff_options opts;
	git_oid oid;
	char str[GIT_OID_HEXSZ + 1];
} change_info;

int main(int argc, char *argv[])
{
	const char *repodir = ".";
	change_info info = { GIT_DIFF_OPTIONS_INIT };
	int start_pathspec = 1;
	size_t i;
	git_repository *repo;
	git_revwalk *walker;

	git_threads_init();

	/* allow you to specific a git repo other than the current one */
	if (argc > 1 && !strncmp(argv[1], "--git-dir=", strlen("--git-dir="))) {
		repodir = argv[1] + strlen("--git-dir=");
		start_pathspec++;
	}

	/* convert arguments to a "pathspec" of interesting files */
	info.opts.pathspec.strings = &argv[start_pathspec];
	info.opts.pathspec.count   = argc - start_pathspec;
	if (!info.opts.pathspec.count)
		usage();
	info.opts.ignore_submodules = GIT_SUBMODULE_IGNORE_DIRTY |
		GIT_DIFF_DISABLE_PATHSPEC_MATCH;
	info.opts.notify_cb = mark_pathspec_match;
	info.opts.notify_payload = &info;

	/* create repo and walker */
	check_lg2(git_repository_open_ext(&repo, repodir, 0, NULL),
		"Could not open repository", repodir);
	check_lg2(git_revwalk_new(&walker, repo),
		"Could not create revision walker", NULL);

	/* start at HEAD and walk backwards through time */
	git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
	check_lg2(git_revwalk_push_head(walker),
		"Could not find repository HEAD", NULL);

	while (info.opts.pathspec.count > 0 &&
		   !git_revwalk_next(&info.oid, walker))
	{
		git_commit *commit;
		git_diff *diff;

		git_oid_tostr(info.str, sizeof(info.str), &info.oid);

		check_lg2(git_commit_lookup(&commit, repo, &info.oid),
			"Failed to look up commit", NULL);
		check_lg2(git_diff_commit(&diff, commit, &info.opts),
			"Failed to get diff for commit", NULL);

		/* notification callback will take care of reporting on
		 * items in the diff and reducing the pathspec count
		 */

		git_diff_free(diff);
		git_commit_free(commit);
	}

	for (i = 0; i < info.opts.pathspec.count; ++i) {
		const char *path = info.opts.pathspec.strings[i];
		if (path)
			printf("never found %s\n", path);
	}

	git_revwalk_free(walker);
	git_repository_free(repo);
	git_threads_shutdown();

	return 0;
}

static int mark_pathspec_match(
	const git_diff *diff_so_far,
	const git_diff_delta *delta_to_add,
	const char *matched_pathspec,
	void *payload)
{
	change_info *info = payload;
	git_strarray *paths = &info->opts.pathspec;
	size_t i;
	int found = 0;

	(void)diff_so_far; (void)delta_to_add;

	for (i = 0; i < paths->count; ++i) {
		/* remove matched item from list */
		if (found)
			paths->strings[i - 1] = paths->strings[i];
		else if (!strcmp(paths->strings[i], matched_pathspec))
			found = 1;
	}

	if (found) {
		const char *verb = "modified";
		if (delta_to_add->status == GIT_DELTA_ADDED)
			verb = "added";
		else if (delta_to_add->status == GIT_DELTA_DELETED)
			verb = "deleted";

		printf("%s has %s %s\n", info->str, verb, matched_pathspec);

		paths->count--;
	}

	return 0;
}
