/*
 * libgit2 tree diff fuzzer target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "git2.h"
#include "git2/sys/mempack.h"

#include "standalone_driver.h"
#include "fuzzer_utils.h"

#define UNUSED(x) (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof(x[0]))

#define MAX_DEPTH 3
#define MAX_PATHSPECS 3

/*
 * The input is a list of choices that select the diff options and build
 * two trees.  The new tree is built from the old tree with some changes,
 * so the two trees share subtrees.  Each diff is compared to the same
 * diff with GIT_DIFF_INCLUDE_UNMODIFIED, which reads every subtree; the
 * deltas must be the same except for the unmodified ones.
 */

typedef struct {
	const uint8_t *data;
	size_t size;
} fuzz_input;

static git_repository *repo;
static git_odb_backend *mempack;

/* names that sort next to each other: `foo`, `foo-bar`, `foo.txt`, `foo/` */
static const char *names[] = {
	"a", "b", "foo", "foo.txt", "foo-bar", "foo0", "Foo", "z"
};

static const char *pathspecs[] = {
	"a", "foo", "foo/", "foo*", "*.txt", "!foo", "*/a", "Foo/b", "foo-bar/a/b"
};

static const unsigned int flags[] = {
	GIT_DIFF_REVERSE,
	GIT_DIFF_INCLUDE_TYPECHANGE,
	GIT_DIFF_INCLUDE_TYPECHANGE_TREES,
	GIT_DIFF_IGNORE_FILEMODE,
	GIT_DIFF_IGNORE_SUBMODULES,
	GIT_DIFF_IGNORE_CASE,
	GIT_DIFF_INCLUDE_CASECHANGE,
	GIT_DIFF_DISABLE_PATHSPEC_MATCH
};

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	git_odb *odb;

	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();

	repo = fuzzer_repo_init();

	/* keep the objects in memory and drop them after each input */
	if (git_repository_odb(&odb, repo) < 0 ||
	    git_mempack_new(&mempack) < 0 ||
	    git_odb_add_backend(odb, mempack, 999) < 0)
		fuzzer_git_abort("mempack");

	git_odb_free(odb);
	return 0;
}

/* return a choice in [0, max), or 0 at the end of the input */
static unsigned int choose(fuzz_input *in, size_t max)
{
	unsigned int value;

	if (in->size == 0)
		return 0;

	value = (unsigned int)(in->data[0] % max);
	in->data++;
	in->size--;

	return value;
}

static void write_tree(
	git_oid *out, fuzz_input *in, const git_tree *base, int depth);

static void insert_entry(
	git_treebuilder *bld, fuzz_input *in, const char *name, int depth)
{
	unsigned char raw[GIT_OID_SHA1_SIZE] = { 0 };
	unsigned int kind = choose(in, depth < MAX_DEPTH ? 5 : 4);
	unsigned int value = choose(in, 4);
	git_filemode_t mode;
	char content[16];
	git_oid id;

	switch (kind) {
	case 0:
		mode = GIT_FILEMODE_BLOB;
		break;
	case 1:
		mode = GIT_FILEMODE_BLOB_EXECUTABLE;
		break;
	case 2:
		mode = GIT_FILEMODE_LINK;
		break;
	case 3:
		mode = GIT_FILEMODE_COMMIT;
		break;
	default:
		mode = GIT_FILEMODE_TREE;
		break;
	}

	if (mode == GIT_FILEMODE_TREE) {
		write_tree(&id, in, NULL, depth + 1);
	} else if (mode == GIT_FILEMODE_COMMIT) {
		/* a submodule commit does not need to exist */
		raw[0] = (unsigned char)(value + 1);

		if (git_oid_from_raw(&id, raw, GIT_OID_SHA1) < 0)
			fuzzer_git_abort("git_oid_from_raw");
	} else {
		sprintf(content, "%u\n", value);

		if (git_blob_create_from_buffer(&id, repo, content, strlen(content)) < 0)
			fuzzer_git_abort("git_blob_create_from_buffer");
	}

	if (git_treebuilder_insert(NULL, bld, name, &id, mode) < 0)
		fuzzer_git_abort("git_treebuilder_insert");
}

static void change_entry(
	git_treebuilder *bld,
	fuzz_input *in,
	const git_tree_entry *entry,
	int depth)
{
	const char *name = git_tree_entry_name(entry);
	const char *new_name;
	git_tree *subtree;
	git_oid id;

	switch (choose(in, 5)) {
	case 0:
		/* keep the entry */
		break;
	case 1:
		if (git_treebuilder_remove(bld, name) < 0)
			fuzzer_git_abort("git_treebuilder_remove");
		break;
	case 2:
		insert_entry(bld, in, name, depth);
		break;
	case 3:
		if (git_tree_entry_type(entry) != GIT_OBJECT_TREE)
			break;

		if (git_tree_lookup(&subtree, repo, git_tree_entry_id(entry)) < 0)
			fuzzer_git_abort("git_tree_lookup");

		write_tree(&id, in, subtree, depth + 1);
		git_tree_free(subtree);

		if (git_treebuilder_insert(NULL, bld, name, &id, GIT_FILEMODE_TREE) < 0)
			fuzzer_git_abort("git_treebuilder_insert");
		break;
	default:
		new_name = names[choose(in, COUNT_OF(names))];

		if (strcmp(name, new_name) == 0)
			break;

		if (git_treebuilder_insert(NULL, bld, new_name,
				git_tree_entry_id(entry),
				git_tree_entry_filemode_raw(entry)) < 0 ||
		    git_treebuilder_remove(bld, name) < 0)
			fuzzer_git_abort("git_treebuilder_insert");
		break;
	}
}

static void write_tree(
	git_oid *out, fuzz_input *in, const git_tree *base, int depth)
{
	git_treebuilder *bld;
	size_t i, count;

	if (git_treebuilder_new(&bld, repo, base) < 0)
		fuzzer_git_abort("git_treebuilder_new");

	for (i = 0; base && i < git_tree_entrycount(base); i++)
		change_entry(bld, in, git_tree_entry_byindex(base, i), depth);

	count = choose(in, base ? 3 : 7);

	for (i = 0; i < count; i++)
		insert_entry(bld, in, names[choose(in, COUNT_OF(names))], depth);

	if (git_treebuilder_write(out, bld) < 0)
		fuzzer_git_abort("git_treebuilder_write");

	git_treebuilder_free(bld);
}

static int skip_deltas_cb(
	const git_diff *diff,
	const git_diff_delta *delta,
	const char *matched_pathspec,
	void *payload)
{
	size_t len = strlen(delta->old_file.path);

	UNUSED(diff);
	UNUSED(matched_pathspec);
	UNUSED(payload);

	/* skip the files named `b` */
	return len > 0 && delta->old_file.path[len - 1] == 'b';
}

static int files_equal(const git_diff_file *a, const git_diff_file *b)
{
	return strcmp(a->path, b->path) == 0 &&
		git_oid_equal(&a->id, &b->id) &&
		a->size == b->size &&
		a->flags == b->flags &&
		a->mode == b->mode;
}

static void deltas_differ(const git_diff_delta *a, const git_diff_delta *b)
{
	fprintf(stderr, "diff: %c %s\n",
		a ? git_diff_status_char(a->status) : '-',
		a ? a->old_file.path : "(none)");
	fprintf(stderr, "diff with unmodified files: %c %s\n",
		b ? git_diff_status_char(b->status) : '-',
		b ? b->old_file.path : "(none)");
	abort();
}

static void compare_diffs(
	git_tree *old_tree, git_tree *new_tree, const git_diff_options *opts)
{
	git_diff_options unmodified_opts;
	git_diff *diff = NULL, *unmodified_diff = NULL;
	const git_diff_delta *a, *b;
	size_t i = 0, j = 0, count, unmodified_count;
	int error, unmodified_error;

	memcpy(&unmodified_opts, opts, sizeof(git_diff_options));
	unmodified_opts.flags |= GIT_DIFF_INCLUDE_UNMODIFIED;

	error = git_diff_tree_to_tree(&diff, repo,
		old_tree, new_tree, opts);
	unmodified_error = git_diff_tree_to_tree(&unmodified_diff, repo,
		old_tree, new_tree, &unmodified_opts);

	if ((error < 0) != (unmodified_error < 0)) {
		fprintf(stderr, "diff errors differ: %d and %d\n",
			error, unmodified_error);
		abort();
	}

	if (error < 0)
		goto done;

	count = git_diff_num_deltas(diff);
	unmodified_count = git_diff_num_deltas(unmodified_diff);

	for (;;) {
		while (j < unmodified_count &&
		       git_diff_get_delta(unmodified_diff, j)->status == GIT_DELTA_UNMODIFIED)
			j++;

		a = (i < count) ? git_diff_get_delta(diff, i++) : NULL;
		b = (j < unmodified_count) ? git_diff_get_delta(unmodified_diff, j++) : NULL;

		if (!a && !b)
			break;

		if (!a || !b ||
		    a->status != b->status ||
		    a->nfiles != b->nfiles ||
		    !files_equal(&a->old_file, &b->old_file) ||
		    !files_equal(&a->new_file, &b->new_file))
			deltas_differ(a, b);
	}

done:
	git_diff_free(diff);
	git_diff_free(unmodified_diff);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
	char *pathspec[MAX_PATHSPECS];
	git_tree *old_tree, *new_tree;
	git_oid old_id, new_id;
	fuzz_input in;
	unsigned int flag_bits;
	size_t i;

	in.data = data;
	in.size = size;

	if (git_mempack_reset(mempack) < 0)
		fuzzer_git_abort("git_mempack_reset");

	flag_bits = choose(&in, 256);

	for (i = 0; i < COUNT_OF(flags); i++) {
		if (flag_bits & (1u << i))
			opts.flags |= flags[i];
	}

	opts.pathspec.count = choose(&in, MAX_PATHSPECS + 1);
	opts.pathspec.strings = pathspec;

	for (i = 0; i < opts.pathspec.count; i++)
		pathspec[i] = (char *)pathspecs[choose(&in, COUNT_OF(pathspecs))];

	if (choose(&in, 2))
		opts.notify_cb = skip_deltas_cb;

	write_tree(&old_id, &in, NULL, 0);

	if (git_tree_lookup(&old_tree, repo, &old_id) < 0)
		fuzzer_git_abort("git_tree_lookup");

	write_tree(&new_id, &in, old_tree, 0);

	if (git_tree_lookup(&new_tree, repo, &new_id) < 0)
		fuzzer_git_abort("git_tree_lookup");

	compare_diffs(old_tree, new_tree, &opts);
	compare_diffs(new_tree, old_tree, &opts);
	compare_diffs(NULL, new_tree, &opts);

	git_tree_free(old_tree);
	git_tree_free(new_tree);

	return 0;
}
