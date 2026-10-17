#include "clar.h"

#include <stdio.h>
#include <string.h>

#include <git2.h>
#include <git2/sys/mempack.h>

/*
 * The trees have BENCHMARK_DIFF_WIDTH directories, each with
 * BENCHMARK_DIFF_WIDTH directories of BENCHMARK_DIFF_WIDTH files.
 */
#define BENCHMARK_DIFF_WIDTH 32

static git_repository *repo;
static git_tree *base_tree, *changed_tree;

static void write_tree(
	git_oid *out, const char *prefix, int depth, const char *changed_path)
{
	git_treebuilder *builder;
	char name[16], path[64];
	const char *content;
	git_oid id;
	int i;

	cl_must_pass(git_treebuilder_new(&builder, repo, NULL));

	for (i = 0; i < BENCHMARK_DIFF_WIDTH; i++) {
		if (depth < 2) {
			sprintf(name, "dir%02d", i);
			sprintf(path, "%s%s/", prefix, name);

			write_tree(&id, path, depth + 1, changed_path);
			cl_must_pass(git_treebuilder_insert(NULL, builder,
				name, &id, GIT_FILEMODE_TREE));
		} else {
			sprintf(name, "file%02d", i);
			sprintf(path, "%s%s", prefix, name);

			content = strcmp(path, changed_path) ? path : "changed";

			cl_must_pass(git_blob_create_from_buffer(&id, repo,
				content, strlen(content)));
			cl_must_pass(git_treebuilder_insert(NULL, builder,
				name, &id, GIT_FILEMODE_BLOB));
		}
	}

	cl_must_pass(git_treebuilder_write(out, builder));
	git_treebuilder_free(builder);
}

void benchmark_diff__initialize(void)
{
	git_odb_backend *mempack;
	git_odb *odb;
	git_oid id;

	cl_must_pass(git_repository_init(&repo, "diff.git", 1));

	/* keep the objects in memory, so that writing them is fast */
	cl_must_pass(git_repository_odb(&odb, repo));
	cl_must_pass(git_mempack_new(&mempack));
	cl_must_pass(git_odb_add_backend(odb, mempack, 999));
	git_odb_free(odb);

	write_tree(&id, "", 0, "");
	cl_must_pass(git_tree_lookup(&base_tree, repo, &id));

	write_tree(&id, "", 0, "dir15/dir15/file15");
	cl_must_pass(git_tree_lookup(&changed_tree, repo, &id));
}

void benchmark_diff__reset(void)
{
}

void benchmark_diff__cleanup(void)
{
	git_tree_free(base_tree);
	git_tree_free(changed_tree);
	git_repository_free(repo);
}

static void diff_trees(git_tree *old_tree, git_tree *new_tree, size_t expected)
{
	git_diff *diff;

	cl_must_pass(git_diff_tree_to_tree(&diff, repo, old_tree, new_tree, NULL));
	cl_assert_equal_i(expected, git_diff_num_deltas(diff));

	git_diff_free(diff);
}

void benchmark_diff__tree_to_tree_unchanged(void)
{
	diff_trees(base_tree, base_tree, 0);
}

void benchmark_diff__tree_to_tree_one_file_changed(void)
{
	diff_trees(base_tree, changed_tree, 1);
}
