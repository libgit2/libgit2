/*
 * libgit2 "reset" example - shows how to reset HEAD and working tree.
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

struct reset_state {
	git_repository *repo;
	const char *repodir;
};

struct reset_options {
	int quiet;
	git_reset_t reset_type;
	const char *rev;
};

static int parse_options(
	struct reset_state *s, struct reset_options *opt, int argc, char **argv);
static void init_repo(struct reset_state *s);

int main(int argc, char *argv[])
{
	int last_arg;
	struct reset_state s;
	struct reset_options opt;
	git_object *revision;

	git_libgit2_init();

	last_arg = parse_options(&s, &opt, argc, argv);

	init_repo(&s);

	check_lg2(git_revparse_single(&revision, s.repo, opt.rev),
		"Could find revision", opt.rev);

	check_lg2(git_reset(s.repo, revision, opt.reset_type, NULL),
		"Reset failed", NULL);

	git_object_free(revision);
	git_repository_free(s.repo);
	git_libgit2_shutdown();

	return 0;
}

static void init_repo(struct reset_state *s)
{
	if (!s->repodir) s->repodir = ".";
	check_lg2(git_repository_open_ext(&s->repo, s->repodir, 0, NULL),
		"Could not open repository", s->repodir);
}

static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: reset [<mode>] [<commit>]\n");
	exit(1);
}

static int parse_options(
	struct reset_state *s, struct reset_options *opt, int argc, char **argv)
{
	struct args_info args = ARGS_INFO_INIT;

	memset(s, 0, sizeof(*s));

	memset(opt, 0, sizeof(*opt));
	opt->rev = "HEAD";
	opt->reset_type = GIT_RESET_MIXED;

	for (args.pos = 1; args.pos < argc; ++args.pos) {
		const char *a = argv[args.pos];

		if (a[0] != '-') {
			/**
			 * Options have to be in front of any revision/commit/filepath.
			 * Only support `git reset [<type>] [<commit>]` for now.
			 **/
			opt->rev = a;
			break;
		} else if (!strcmp(a, "-q") || !strcmp(a, "--quiet"))
			opt->quiet = 1;
		else if (!strcmp(a, "--mixed"))
			opt->reset_type = GIT_RESET_MIXED;
		else if (!strcmp(a, "--soft"))
			opt->reset_type = GIT_RESET_SOFT;
		else if (!strcmp(a, "--hard"))
			opt->reset_type = GIT_RESET_HARD;
		else
			usage("Unsupported argument", a);
	}

	return args.pos;
}
