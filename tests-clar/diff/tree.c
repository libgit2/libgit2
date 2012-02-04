#include "clar_libgit2.h"
#include "diff_helpers.h"

static git_repository *g_repo = NULL;

void test_diff_tree__initialize(void)
{
	cl_fixture_sandbox("attr");
	cl_git_pass(p_rename("attr/.gitted", "attr/.git"));
	cl_git_pass(p_rename("attr/gitattributes", "attr/.gitattributes"));
	cl_git_pass(git_repository_open(&g_repo, "attr/.git"));
}

void test_diff_tree__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup("attr");
}

static git_tree *resolve_commit_oid_to_tree(const char *partial_oid)
{
	size_t len = strlen(partial_oid);
	git_oid oid;
	git_object *obj;
	git_tree *tree;

	if (git_oid_fromstrn(&oid, partial_oid, len) == 0)
		git_object_lookup_prefix(&obj, g_repo, &oid, len, GIT_OBJ_ANY);
	cl_assert(obj);
	if (git_object_type(obj) == GIT_OBJ_TREE)
		return (git_tree *)obj;
	cl_assert(git_object_type(obj) == GIT_OBJ_COMMIT);
	cl_git_pass(git_commit_tree(&tree, (git_commit *)obj));
	git_object_free(obj);
	return tree;
}

void test_diff_tree__0(void)
{
	/* grabbed a couple of commit oids from the history of the attr repo */
	const char *a_commit = "605812a";
	const char *b_commit = "370fe9ec22";
	const char *c_commit = "f5b0af1fb4f5c";
	git_tree *a = resolve_commit_oid_to_tree(a_commit);
	git_tree *b = resolve_commit_oid_to_tree(b_commit);
	git_tree *c = resolve_commit_oid_to_tree(c_commit);
	git_diff_options opts;
	git_diff_list *diff = NULL;
	diff_expects exp;

	cl_assert(a);
	cl_assert(b);

	opts.context_lines = 1;
	opts.interhunk_lines = 0;
	opts.ignore_whitespace = 0;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, a, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 5);
	cl_assert(exp.file_adds == 2);
	cl_assert(exp.file_dels == 1);
	cl_assert(exp.file_mods == 2);

	cl_assert(exp.hunks == 5);

	cl_assert(exp.lines == 7 + 24 + 1 + 6 + 6);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 24 + 1 + 5 + 5);
	cl_assert(exp.line_dels == 7 + 1);

	git_diff_list_free(diff);
	diff = NULL;

	memset(&exp, 0, sizeof(exp));

	cl_git_pass(git_diff_tree_to_tree(g_repo, &opts, c, b, &diff));

	cl_git_pass(git_diff_foreach(
		diff, &exp, diff_file_fn, diff_hunk_fn, diff_line_fn));

	cl_assert(exp.files == 2);
	cl_assert(exp.file_adds == 0);
	cl_assert(exp.file_dels == 0);
	cl_assert(exp.file_mods == 2);

	cl_assert(exp.hunks == 2);

	cl_assert(exp.lines == 8 + 15);
	cl_assert(exp.line_ctxt == 1);
	cl_assert(exp.line_adds == 1);
	cl_assert(exp.line_dels == 7 + 14);

	git_diff_list_free(diff);

	git_tree_free(a);
	git_tree_free(b);
	git_tree_free(c);
}
