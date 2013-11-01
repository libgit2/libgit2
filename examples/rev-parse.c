/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

/** Forward declarations for helpers. */
struct parse_state {
	git_repository *repo;
	const char *repodir;
	const char *spec;
	int not;
};
static void parse_opts(struct parse_state *ps, int argc, char *argv[]);
static int parse_revision(struct parse_state *ps);


int main(int argc, char *argv[])
{
	struct parse_state ps = {0};

	git_threads_init();
	parse_opts(&ps, argc, argv);

	check_lg2(parse_revision(&ps), "Parsing", NULL);

	git_repository_free(ps.repo);
	git_threads_shutdown();

	return 0;
}

static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: rev-parse [ --option ] <args>...\n");
	exit(1);
}

static void parse_opts(struct parse_state *ps, int argc, char *argv[])
{
	struct args_info args = ARGS_INFO_INIT;

	for (args.pos=1; args.pos < argc; ++args.pos) {
		const char *a = argv[args.pos];

		if (a[0] != '-') {
			if (ps->spec)
				usage("Too many specs", a);
			ps->spec = a;
		} else if (!strcmp(a, "--not"))
			ps->not = !ps->not;
		else if (!match_str_arg(&ps->repodir, &args, "--git-dir"))
			usage("Cannot handle argument", a);
	}
}

static int parse_revision(struct parse_state *ps)
{
	git_revspec rs;
	char str[GIT_OID_HEXSZ + 1];

	if (!ps->repo) {
		if (!ps->repodir)
			ps->repodir = ".";
		check_lg2(git_repository_open_ext(&ps->repo, ps->repodir, 0, NULL),
			"Could not open repository from", ps->repodir);
	}

	check_lg2(git_revparse(&rs, ps->repo, ps->spec), "Could not parse", ps->spec);

	if ((rs.flags & GIT_REVPARSE_SINGLE) != 0) {
		git_oid_tostr(str, sizeof(str), git_object_id(rs.from));
		printf("%s\n", str);
		git_object_free(rs.from);
	}
	else if ((rs.flags & GIT_REVPARSE_RANGE) != 0) {
		git_oid_tostr(str, sizeof(str), git_object_id(rs.to));
		printf("%s\n", str);
		git_object_free(rs.to);

		if ((rs.flags & GIT_REVPARSE_MERGE_BASE) != 0) {
			git_oid base;
			check_lg2(git_merge_base(&base, ps->repo,
						git_object_id(rs.from), git_object_id(rs.to)),
					"Could not find merge base", ps->spec);

			git_oid_tostr(str, sizeof(str), &base);
			printf("%s\n", str);
		}

		git_oid_tostr(str, sizeof(str), git_object_id(rs.from));
		printf("^%s\n", str);
		git_object_free(rs.from);
	}
	else {
		fatal("Invalid results from git_revparse", ps->spec);
	}

	return 0;
}

