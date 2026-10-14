#include "pathspec_helpers.h"

static char *pattern_readme[] = { "README" };
static char *pattern_wildcard_txt[] = { "*.txt" };
static char *pattern_no_match[] = { "does-not-exist.xyz" };
static char *pattern_directory_ab[] = { "ab" };
static char *pattern_wildcard_with_prefix[] = { "ab/*.txt" };
static char *pattern_multiple[] = { "README", "*.txt" };
static char *pattern_branch_file[] = { "branch_file.txt" };

/**
 * $ git log -- README
 * Lists commits
 * 4a202b346bb0fb0db7eff3cffeb3c70babbd2045
 * 8496071c1b46c854b31185ea97743be6a8774479
 */
const char *pathspec_expected_exact_str[2] = {
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045",
	"8496071c1b46c854b31185ea97743be6a8774479",
};

/**
 * $ git log -- *.txt
 * Lists commits
 * a65fedf39aefe402d3bb6e24df4d4f5fe4547750
 * be3563ae3f795b2b4353bcce3a527ad0a4f7f644
 * c47800c7266a2be04c571c04d5a6614691ea99bd
 * 9fd738e8f7967c078dceed8190330fc8648ee56a
 * 5b5b025afb0b4c913b4c338a42934a3863bf3644
 */
static const char *expected_wildcard_str[] = {
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750",
	"be3563ae3f795b2b4353bcce3a527ad0a4f7f644",
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
	"9fd738e8f7967c078dceed8190330fc8648ee56a",
	"5b5b025afb0b4c913b4c338a42934a3863bf3644",
};

/**
 * $ git log --oneline subtrees -- ab
 * Lists commit
 * 763d71aadf09a7951596c9746c024e7eece7c7af
 */
static const char *expected_directory_str[] = {
	"763d71aadf09a7951596c9746c024e7eece7c7af",
};

/**
 * $ git log --oneline HEAD -- README '*.txt'
 * Lists the union of the exact_file and wildcard commits, which happens to
 * be every commit reachable from HEAD in this fixture.
 *
 * $ git rev-list --count HEAD
 * 7
 */
const char *pathspec_expected_all_str[7] = {
	"a65fedf39aefe402d3bb6e24df4d4f5fe4547750",
	"be3563ae3f795b2b4353bcce3a527ad0a4f7f644",
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
	"9fd738e8f7967c078dceed8190330fc8648ee56a",
	"4a202b346bb0fb0db7eff3cffeb3c70babbd2045",
	"5b5b025afb0b4c913b4c338a42934a3863bf3644",
	"8496071c1b46c854b31185ea97743be6a8774479",
};

/**
 * be3563a "Merge branch 'br2'" only touches branch_file.txt relative to one
 * of its two parents (it is unchanged versus c47800c). It must not show up
 * when walking history for that path, since a merge only counts as a change
 * when it differs from *every* parent.
 *
 * $ git log --oneline refs/remotes/test/master -- branch_file.txt
 * Lists commit
 * c47800c7266a2be04c571c04d5a6614691ea99bd
 */
static const char *expected_merge_single_parent_str[] = {
	"c47800c7266a2be04c571c04d5a6614691ea99bd",
};

const pathspec_case pathspec_cases[] = {
	{ "exact_file", NULL, pattern_readme, 1,
		pathspec_expected_exact_str, 2 },
	{ "wildcard", NULL, pattern_wildcard_txt, 1,
		expected_wildcard_str, 5 },
	{ "no_match", NULL, pattern_no_match, 1,
		NULL, 0 },
	{ "directory", "refs/heads/subtrees", pattern_directory_ab, 1,
		expected_directory_str, 1 },
	{ "wildcard_with_prefix", "refs/heads/subtrees", pattern_wildcard_with_prefix, 1,
		expected_directory_str, 1 },
	{ "multiple_patterns", NULL, pattern_multiple, 2,
		pathspec_expected_all_str, 7 },
	{ "merge_commit_single_parent_touches", "refs/remotes/test/master",
		pattern_branch_file, 1, expected_merge_single_parent_str, 1 },
};
const size_t pathspec_cases_count = ARRAY_SIZE(pathspec_cases);

void pathspec_assert_walk(
	git_repository *repo,
	const char *name,
	const char *pushref,
	char **patterns,
	size_t pattern_count,
	const char **expected_oid_strs,
	size_t expected_count)
{
	git_revwalk *walk;
	git_pathspec *ps = NULL;
	git_oid id, expected;
	char id_str[GIT_OID_MAX_HEXSIZE + 1], expected_str[GIT_OID_MAX_HEXSIZE + 1];
	size_t i;
	int error;
	git_strarray paths = { NULL, 0 };
	paths.strings = patterns;
	paths.count = pattern_count;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_pathspec_new(&ps, &paths));
	cl_git_pass(git_revwalk_pathspec(walk, ps));

	if (pushref)
		cl_git_pass(git_revwalk_push_ref(walk, pushref));
	else
		cl_git_pass(git_revwalk_push_head(walk));

	i = 0;
	while ((error = git_revwalk_next(&id, walk)) == 0) {
		if (i >= expected_count) {
			git_oid_tostr(id_str, sizeof(id_str), &id);
			cl_failf("case '%s': unexpected extra result %s", name, id_str);
		}

		cl_git_pass(git_oid_from_string(&expected, expected_oid_strs[i], GIT_OID_SHA1));
		if (!git_oid_equal(&expected, &id)) {
			git_oid_tostr(id_str, sizeof(id_str), &id);
			git_oid_tostr(expected_str, sizeof(expected_str), &expected);
			cl_failf("case '%s': result %d: expected %s, got %s",
				name, (int)i, expected_str, id_str);
		}
		i++;
	}

	if (i != expected_count)
		cl_failf("case '%s': expected %d results, got %d",
			name, (int)expected_count, (int)i);
	if (error != GIT_ITEROVER)
		cl_failf("case '%s': walk ended with error %d, expected GIT_ITEROVER",
			name, error);

	git_revwalk_free(walk);
	git_pathspec_free(ps);
}

void pathspec_run_cases(git_repository *repo)
{
	size_t i;

	for (i = 0; i < pathspec_cases_count; i++) {
		const pathspec_case *c = &pathspec_cases[i];
		pathspec_assert_walk(repo, c->name, c->pushref,
			c->patterns, c->pattern_count,
			c->expected_oid_strs, c->expected_count);
	}
}
