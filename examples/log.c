#include <stdio.h>
#include <git2.h>
#include <stdlib.h>
#include <string.h>

static void check(int error, const char *message, const char *arg)
{
	if (!error)
		return;
	if (arg)
		fprintf(stderr, "%s '%s' (%d)\n", message, arg, error);
	else
		fprintf(stderr, "%s (%d)\n", message, error);
	exit(1);
}

static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: log [<options>]\n");
	exit(1);
}

struct log_state {
	git_repository *repo;
	const char *repodir;
	git_revwalk *walker;
	int hide;
	int sorting;
};

static void set_sorting(struct log_state *s, unsigned int sort_mode)
{
	if (!s->repo) {
		if (!s->repodir) s->repodir = ".";
		check(git_repository_open_ext(&s->repo, s->repodir, 0, NULL),
			"Could not open repository", s->repodir);
	}

	if (!s->walker)
		check(git_revwalk_new(&s->walker, s->repo),
			"Could not create revision walker", NULL);

	if (sort_mode == GIT_SORT_REVERSE)
		s->sorting = s->sorting ^ GIT_SORT_REVERSE;
	else
		s->sorting = sort_mode | (s->sorting & GIT_SORT_REVERSE);

	git_revwalk_sorting(s->walker, s->sorting);
}

static void push_rev(struct log_state *s, git_object *obj, int hide)
{
	hide = s->hide ^ hide;

	if (!s->walker) {
		check(git_revwalk_new(&s->walker, s->repo),
			"Could not create revision walker", NULL);
		git_revwalk_sorting(s->walker, s->sorting);
	}

	if (!obj)
		check(git_revwalk_push_head(s->walker),
			"Could not find repository HEAD", NULL);
	else if (hide)
		check(git_revwalk_hide(s->walker, git_object_id(obj)),
			"Reference does not refer to a commit", NULL);
	else
		check(git_revwalk_push(s->walker, git_object_id(obj)),
			"Reference does not refer to a commit", NULL);

	git_object_free(obj);
}

static int add_revision(struct log_state *s, const char *revstr)
{
	git_revspec revs;
	int hide = 0;

	if (!s->repo) {
		if (!s->repodir) s->repodir = ".";
		check(git_repository_open_ext(&s->repo, s->repodir, 0, NULL),
			"Could not open repository", s->repodir);
	}

	if (!revstr) {
		push_rev(s, NULL, hide);
		return 0;
	}

	if (*revstr == '^') {
		revs.flags = GIT_REVPARSE_SINGLE;
		hide = !hide;

		if (git_revparse_single(&revs.from, s->repo, revstr + 1) < 0)
			return -1;
	} else if (git_revparse(&revs, s->repo, revstr) < 0)
		return -1;

	if ((revs.flags & GIT_REVPARSE_SINGLE) != 0)
		push_rev(s, revs.from, hide);
	else {
		push_rev(s, revs.to, hide);

		if ((revs.flags & GIT_REVPARSE_MERGE_BASE) != 0) {
			git_oid base;
			check(git_merge_base(&base, s->repo,
				git_object_id(revs.from), git_object_id(revs.to)),
				"Could not find merge base", revstr);
			check(git_object_lookup(&revs.to, s->repo, &base, GIT_OBJ_COMMIT),
				"Could not find merge base commit", NULL);

			push_rev(s, revs.to, hide);
		}

		push_rev(s, revs.from, !hide);
	}

	return 0;
}

static void print_time(const git_time *intime, const char *prefix)
{
	char sign, out[32];
	struct tm intm;
	int offset, hours, minutes;
	time_t t;

	offset = intime->offset;
	if (offset < 0) {
		sign = '-';
		offset = -offset;
	} else {
		sign = '+';
	}

	hours   = offset / 60;
	minutes = offset % 60;

	t = (time_t)intime->time + (intime->offset * 60);

	gmtime_r(&t, &intm);
	strftime(out, sizeof(out), "%a %b %e %T %Y", &intm);

	printf("%s%s %c%02d%02d\n", prefix, out, sign, hours, minutes);
}

static void print_commit(git_commit *commit)
{
	char buf[GIT_OID_HEXSZ + 1];
	int i, count;
	const git_signature *sig;
	const char *scan, *eol;

	git_oid_tostr(buf, sizeof(buf), git_commit_id(commit));
	printf("commit %s\n", buf);

	if ((count = (int)git_commit_parentcount(commit)) > 1) {
		printf("Merge:");
		for (i = 0; i < count; ++i) {
			git_oid_tostr(buf, 8, git_commit_parent_id(commit, i));
			printf(" %s", buf);
		}
		printf("\n");
	}

	if ((sig = git_commit_author(commit)) != NULL) {
		printf("Author: %s <%s>\n", sig->name, sig->email);
		print_time(&sig->when, "Date:   ");
	}
	printf("\n");

	for (scan = git_commit_message(commit); scan && *scan; ) {
		for (eol = scan; *eol && *eol != '\n'; ++eol) /* find eol */;

		printf("    %.*s\n", (int)(eol - scan), scan);
		scan = *eol ? eol + 1 : NULL;
	}
	printf("\n");
}

static int print_diff(
	const git_diff_delta *delta,
	const git_diff_range *range,
	char usage,
	const char *line,
	size_t line_len,
	void *data)
{
	(void)delta; (void)range; (void)usage; (void)line_len; (void)data;
	fputs(line, stdout);
	return 0;
}

static int match_int(int *value, const char *arg, int allow_negative)
{
	char *found;
	*value = (int)strtol(arg, &found, 10);
	return (found && *found == '\0' && (allow_negative || *value >= 0));
}

static int match_int_arg(
	int *value, const char *arg, const char *pfx, int allow_negative)
{
	size_t pfxlen = strlen(pfx);
	if (strncmp(arg, pfx, pfxlen) != 0)
		return 0;
	if (!match_int(value, arg + pfxlen, allow_negative))
		usage("Invalid value after argument", arg);
	return 1;
}

static int match_with_parent(
	git_commit *commit, int i, git_diff_options *opts)
{
	git_commit *parent;
	git_tree *a, *b;
	git_diff_list *diff;
	int ndeltas;

	check(git_commit_parent(&parent, commit, (size_t)i), "Get parent", NULL);
	check(git_commit_tree(&a, parent), "Tree for parent", NULL);
	check(git_commit_tree(&b, commit), "Tree for commit", NULL);
	check(git_diff_tree_to_tree(&diff, git_commit_owner(commit), a, b, opts),
		  "Checking diff between parent and commit", NULL);

	ndeltas = (int)git_diff_num_deltas(diff);

	git_diff_list_free(diff);
	git_tree_free(a);
	git_tree_free(b);
	git_commit_free(parent);

	return ndeltas > 0;
}

struct log_options {
	int show_diff;
	int skip, limit;
	int min_parents, max_parents;
	git_time_t before;
	git_time_t after;
	char *author;
	char *committer;
};

int main(int argc, char *argv[])
{
	int i, count = 0, printed = 0, parents;
	char *a;
	struct log_state s;
	struct log_options opt;
	git_diff_options diffopts = GIT_DIFF_OPTIONS_INIT;
	git_oid oid;
	git_commit *commit = NULL;
	git_pathspec *ps = NULL;

	git_threads_init();

	memset(&s, 0, sizeof(s));
	s.sorting = GIT_SORT_TIME;

	memset(&opt, 0, sizeof(opt));
	opt.max_parents = -1;
 	opt.limit = -1;

	for (i = 1; i < argc; ++i) {
		a = argv[i];

		if (a[0] != '-') {
			if (!add_revision(&s, a))
				++count;
			else /* try failed revision parse as filename */
				break;
		} else if (!strcmp(a, "--")) {
			++i;
			break;
		}
		else if (!strcmp(a, "--date-order"))
			set_sorting(&s, GIT_SORT_TIME);
		else if (!strcmp(a, "--topo-order"))
			set_sorting(&s, GIT_SORT_TOPOLOGICAL);
		else if (!strcmp(a, "--reverse"))
			set_sorting(&s, GIT_SORT_REVERSE);
		else if (!strncmp(a, "--git-dir=", strlen("--git-dir=")))
			s.repodir = a + strlen("--git-dir=");
		else if (match_int_arg(&opt.skip, a, "--skip=", 0))
			/* found valid --skip */;
		else if (match_int_arg(&opt.limit, a, "--max-count=", 0))
			/* found valid --max-count */;
		else if (a[1] >= '0' && a[1] <= '9') {
			if (!match_int(&opt.limit, a + 1, 0))
				usage("Invalid limit on number of commits", a);
		} else if (!strcmp(a, "-n")) {
			if (i + 1 == argc || !match_int(&opt.limit, argv[i + 1], 0))
				usage("Argument -n not followed by valid count", argv[i + 1]);
			else
				++i;
		}
		else if (!strcmp(a, "--merges"))
			opt.min_parents = 2;
		else if (!strcmp(a, "--no-merges"))
			opt.max_parents = 1;
		else if (!strcmp(a, "--no-min-parents"))
			opt.min_parents = 0;
		else if (!strcmp(a, "--no-max-parents"))
			opt.max_parents = -1;
		else if (match_int_arg(&opt.max_parents, a, "--max-parents=", 1))
			/* found valid --max-parents */;
		else if (match_int_arg(&opt.min_parents, a, "--min-parents=", 0))
			/* found valid --min_parents */;
		else if (!strcmp(a, "-p") || !strcmp(a, "-u") || !strcmp(a, "--patch"))
			opt.show_diff = 1;
		else
			usage("Unsupported argument", a);
	}

	if (!count)
		add_revision(&s, NULL);

	diffopts.pathspec.strings = &argv[i];
	diffopts.pathspec.count   = argc - i;
	if (diffopts.pathspec.count > 0)
		check(git_pathspec_new(&ps, &diffopts.pathspec),
			"Building pathspec", NULL);

	printed = count = 0;

	for (; !git_revwalk_next(&oid, s.walker); git_commit_free(commit)) {
		check(git_commit_lookup(&commit, s.repo, &oid),
			"Failed to look up commit", NULL);

		parents = (int)git_commit_parentcount(commit);
		if (parents < opt.min_parents)
			continue;
		if (opt.max_parents > 0 && parents > opt.max_parents)
			continue;

		if (diffopts.pathspec.count > 0) {
			int unmatched = parents;

			if (parents == 0) {
				git_tree *tree;
				check(git_commit_tree(&tree, commit), "Get tree", NULL);
				if (git_pathspec_match_tree(
						NULL, tree, GIT_PATHSPEC_NO_MATCH_ERROR, ps) != 0)
					unmatched = 1;
				git_tree_free(tree);
			} else if (parents == 1) {
				unmatched = match_with_parent(commit, 0, &diffopts) ? 0 : 1;
			} else {
				for (i = 0; i < parents; ++i) {
					if (match_with_parent(commit, i, &diffopts))
						unmatched--;
				}
			}

			if (unmatched > 0)
				continue;
		}

		if (count++ < opt.skip)
			continue;
		if (opt.limit != -1 && printed++ >= opt.limit) {
			git_commit_free(commit);
			break;
		}

		print_commit(commit);

		if (opt.show_diff) {
			git_tree *a = NULL, *b = NULL;
			git_diff_list *diff = NULL;

			if (parents > 1)
				continue;
			check(git_commit_tree(&b, commit), "Get tree", NULL);
			if (parents == 1) {
				git_commit *parent;
				check(git_commit_parent(&parent, commit, 0), "Get parent", NULL);
				check(git_commit_tree(&a, parent), "Tree for parent", NULL);
				git_commit_free(parent);
			}

			check(git_diff_tree_to_tree(
				&diff, git_commit_owner(commit), a, b, &diffopts),
				"Diff commit with parent", NULL);
			check(git_diff_print_patch(diff, print_diff, NULL),
				"Displaying diff", NULL);

			git_diff_list_free(diff);
			git_tree_free(a);
			git_tree_free(b);
		}
	}

	git_pathspec_free(ps);
	git_revwalk_free(s.walker);
	git_repository_free(s.repo);
	git_threads_shutdown();

	return 0;
}
