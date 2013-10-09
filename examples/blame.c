#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

static void check(int error, const char *msg)
{
	if (error) {
		fprintf(stderr, "%s (%d)\n", msg, error);
		exit(error);
	}
}

static void usage(const char *msg, const char *arg)
{
	if (msg && arg)
		fprintf(stderr, "%s: %s\n", msg, arg);
	else if (msg)
		fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "usage: blame <path> [options] [<commit range>]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   <commit range>      example: `HEAD~10..HEAD`, or `1234abcd`\n");
	fprintf(stderr, "   -L <n,m>            process only line range n-m, counting from 1\n");
	fprintf(stderr, "   -M                  find line moves within and across files\n");
	fprintf(stderr, "   -C                  find line copies within and across files\n");
	fprintf(stderr, "\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int i;
	char *path = NULL, *a;
	const char *rawdata, *commitspec=NULL;
	git_repository *repo = NULL;
	git_revspec revspec = {0};
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
	git_blame *blame = NULL;
	git_commit *commit;
	git_tree *tree;
	git_tree_entry *entry;
	git_blob *blob;

	git_threads_init();

	if (argc < 2) usage(NULL, NULL);
	path = argv[1];

	for (i=2; i<argc; i++) {
		a = argv[i];

		if (!strcmp(a, "-M"))
			opts.flags |= GIT_BLAME_TRACK_COPIES_SAME_COMMIT_MOVES;
		else if (!strcmp(a, "-C"))
			opts.flags |= GIT_BLAME_TRACK_COPIES_SAME_COMMIT_COPIES;
		else if (!strcmp(a, "-L")) {
			i++; a = argv[i];
			if (i >= argc) check(-1, "Not enough arguments to -L");
			check(sscanf(a, "%d,%d", &opts.min_line, &opts.max_line)-2, "-L format error");
		}
		else {
			/* commit range */
			if (commitspec) check(-1, "Only one commit spec allowed");
			commitspec = a;
		}
	}

	/* Open the repo */
	check(git_repository_open_ext(&repo, ".", 0, NULL), "Couldn't open repository");

	/* Parse the end points */
	if (commitspec) {
		check(git_revparse(&revspec, repo, commitspec), "Couldn't parse commit spec");
		if (revspec.flags & GIT_REVPARSE_SINGLE) {
			git_oid_cpy(&opts.newest_commit, git_object_id(revspec.from));
			git_object_free(revspec.from);
		} else {
			git_oid_cpy(&opts.oldest_commit, git_object_id(revspec.from));
			git_oid_cpy(&opts.newest_commit, git_object_id(revspec.to));
			git_object_free(revspec.from);
			git_object_free(revspec.to);
		}
	}

	/* Run the blame */
	check(git_blame_file(&blame, repo, path, &opts), "Blame error");

	/* Get the raw data for output */
	if (git_oid_iszero(&opts.newest_commit)) {
		git_object *obj;
		check(git_revparse_single(&obj, repo, "HEAD"), "Can't find HEAD");
		git_oid_cpy(&opts.newest_commit, git_object_id(obj));
		git_object_free(obj);
	}
	check(git_commit_lookup(&commit, repo, &opts.newest_commit), "Commit lookup error");
	check(git_commit_tree(&tree, commit), "Commit tree lookup error");
	check(git_tree_entry_bypath(&entry, tree, path), "Tree entry lookup error");
	check(git_blob_lookup(&blob, repo, git_tree_entry_id(entry)), "Blob lookup error");
	rawdata = git_blob_rawcontent(blob);

	/* Produce the output */
	i = 1;
	while (rawdata[0]) {
		const char *eol = strchr(rawdata, '\n');
		char oid[10] = {0};
		const git_blame_hunk *hunk = git_blame_get_hunk_byline(blame, i);
		git_commit *hunkcommit;
		const git_signature *sig;

		git_oid_tostr(oid, 10, &hunk->final_commit_id);
		check(git_commit_lookup(&hunkcommit, repo, &hunk->final_commit_id), "Commit lookup error");
		sig = git_commit_author(hunkcommit);

		printf("%s ( %-30s %3d) %.*s\n",
				oid,
				sig->name,
				i,
				(int)(eol-rawdata),
				rawdata);

		git_commit_free(hunkcommit);
		rawdata = eol+1;
		i++;
	}

	/* Cleanup */
	git_blob_free(blob);
	git_tree_entry_free(entry);
	git_tree_free(tree);
	git_commit_free(commit);
	git_blame_free(blame);
	git_repository_free(repo);
	git_threads_shutdown();
}
