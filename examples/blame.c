/*
 * libgit2 "blame" example - shows how to use the blame API
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
 * This example demonstrates how to invoke the libgit2 blame API to roughly
 * simulate the output of `git blame` and a few of its command line arguments.
 */

static void usage(const char *msg, const char *arg);

int main(int argc, char *argv[])
{
	int i, line, break_on_null_hunk;
	const char *path = NULL, *a;
	const char *rawdata, *commitspec=NULL, *bare_args[3] = {0};
	char spec[1024] = {0};
	git_repository *repo = NULL;
	git_revspec revspec = {0};
	git_blame_options opts = GIT_BLAME_OPTIONS_INIT;
	git_blame *blame = NULL;
	git_blob *blob;

	git_threads_init();

	if (argc < 2) usage(NULL, NULL);

	for (i=1; i<argc; i++) {
		a = argv[i];

		if (a[0] != '-') {
			int i=0;
			while (bare_args[i] && i < 3) ++i;
			if (i >= 3)
				usage("Invalid argument set", NULL);
			bare_args[i] = a;
		}
		else if (!strcmp(a, "--"))
			continue;
		else if (!strcasecmp(a, "-M"))
			opts.flags |= GIT_BLAME_TRACK_COPIES_SAME_COMMIT_MOVES;
		else if (!strcasecmp(a, "-C"))
			opts.flags |= GIT_BLAME_TRACK_COPIES_SAME_COMMIT_COPIES;
		else if (!strcasecmp(a, "-L")) {
			i++; a = argv[i];
			if (i >= argc) fatal("Not enough arguments to -L", NULL);
			check_lg2(sscanf(a, "%d,%d", &opts.min_line, &opts.max_line)-2, "-L format error", NULL);
		}
		else {
			/* commit range */
			if (commitspec) fatal("Only one commit spec allowed", NULL);
			commitspec = a;
		}
	}

	/* Handle the bare arguments */
	if (!bare_args[0]) usage("Please specify a path", NULL);
	path = bare_args[0];
	if (bare_args[1]) {
		/* <commitspec> <path> */
		path = bare_args[1];
		commitspec = bare_args[0];
	}
	if (bare_args[2]) {
		/* <oldcommit> <newcommit> <path> */
		path = bare_args[2];
		sprintf(spec, "%s..%s", bare_args[0], bare_args[1]);
		commitspec = spec;
	}

	/* Open the repo */
	check_lg2(git_repository_open_ext(&repo, ".", 0, NULL), "Couldn't open repository", NULL);

	/* Parse the end points */
	if (commitspec) {
		check_lg2(git_revparse(&revspec, repo, commitspec), "Couldn't parse commit spec", NULL);
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
	check_lg2(git_blame_file(&blame, repo, path, &opts), "Blame error", NULL);

	/* Get the raw data for output */
	if (git_oid_iszero(&opts.newest_commit))
		strcpy(spec, "HEAD");
	else
		git_oid_tostr(spec, sizeof(spec), &opts.newest_commit);
	strcat(spec, ":");
	strcat(spec, path);

	{
		git_object *obj;
		check_lg2(git_revparse_single(&obj, repo, spec), "Object lookup error", NULL);
		check_lg2(git_blob_lookup(&blob, repo, git_object_id(obj)), "Blob lookup error", NULL);
		git_object_free(obj);
	}
	rawdata = git_blob_rawcontent(blob);

	/* Produce the output */
	line = 1;
	i = 0;
	break_on_null_hunk = 0;
	while (i < git_blob_rawsize(blob)) {
		const char *eol = strchr(rawdata+i, '\n');
		char oid[10] = {0};
		const git_blame_hunk *hunk = git_blame_get_hunk_byline(blame, line);

		if (break_on_null_hunk && !hunk) break;

		if (hunk) {
			break_on_null_hunk = 1;
			char sig[128] = {0};

			git_oid_tostr(oid, 10, &hunk->final_commit_id);
			snprintf(sig, 30, "%s <%s>", hunk->final_signature->name, hunk->final_signature->email);

			printf("%s ( %-30s %3d) %.*s\n",
					oid,
					sig,
					line,
					(int)(eol-rawdata-i),
					rawdata+i);
		}

		i = eol - rawdata + 1;
		line++;
	}

	/* Cleanup */
	git_blob_free(blob);
	git_blame_free(blame);
	git_repository_free(repo);
	git_threads_shutdown();
}

static void usage(const char *msg, const char *arg)
{
	if (msg && arg)
		fprintf(stderr, "%s: %s\n", msg, arg);
	else if (msg)
		fprintf(stderr, "%s\n", msg);
	fprintf(stderr, "usage: blame [options] [<commit range>] <path>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   <commit range>      example: `HEAD~10..HEAD`, or `1234abcd`\n");
	fprintf(stderr, "   -L <n,m>            process only line range n-m, counting from 1\n");
	fprintf(stderr, "   -M                  find line moves within and across files\n");
	fprintf(stderr, "   -C                  find line copies within and across files\n");
	fprintf(stderr, "\n");
	exit(1);
}

