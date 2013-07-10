#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

static void check(int error, const char *message, const char *arg)
{
	if (!error)
		return;
	if (arg)
		fprintf(stderr, "%s %s (%d)\n", message, arg, error);
	else
		fprintf(stderr, "%s(%d)\n", message, error);
	exit(1);
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

struct parse_state {
	git_repository *repo;
	const char *repodir;
	int not;
};

static int parse_revision(struct parse_state *ps, const char *revstr)
{
	git_revspec rs;
	char str[GIT_OID_HEXSZ + 1];

	if (!ps->repo) {
		if (!ps->repodir)
			ps->repodir = ".";
		check(git_repository_open_ext(&ps->repo, ps->repodir, 0, NULL),
			"Could not open repository from", ps->repodir);
	}

	check(git_revparse(&rs, ps->repo, revstr), "Could not parse", revstr);

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
			check(git_merge_base(&base, ps->repo,
				git_object_id(rs.from), git_object_id(rs.to)),
				"Could not find merge base", revstr);

			git_oid_tostr(str, sizeof(str), &base);
			printf("%s\n", str);
		}

		git_oid_tostr(str, sizeof(str), git_object_id(rs.from));
		printf("^%s\n", str);
		git_object_free(rs.from);
	}
	else {
		check(0, "Invalid results from git_revparse", revstr);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	char *a;
	struct parse_state ps;

	git_threads_init();

	memset(&ps, 0, sizeof(ps));

	for (i = 1; i < argc; ++i) {
		a = argv[i];

		if (a[0] != '-') {
			if (parse_revision(&ps, a) != 0)
				break;
		} else if (!strcmp(a, "--not"))
			ps.not = !ps.not;
		else if (!strncmp(a, "--git-dir=", strlen("--git-dir=")))
			ps.repodir = a + strlen("--git-dir=");
		else
			usage("Cannot handle argument", a);
	}

	git_repository_free(ps.repo);
	git_threads_shutdown();

	return 0;
}
