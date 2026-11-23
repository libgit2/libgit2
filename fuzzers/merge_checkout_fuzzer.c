/*
 * libgit2 merge + checkout fuzz target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

/*
 * Fuzzes merge analysis/merge machinery and the checkout driver over a
 * hostile repository.
 *
 * Threat model: an application merges or checks out inside a repository
 * whose objects are attacker-controlled (pulled from a hostile remote,
 * unpacked from an archive): object contents and tree entry names are
 * attacker bytes, only hashes are validated. merge.c, checkout.c and the
 * tree/iterator machinery backing them were 100% dark in production
 * coverage (2026-11-05): no existing target reaches them.
 *
 * The input is a small opcode program run against a fresh non-bare repo
 * fixture per iteration (checkout needs a workdir):
 *   op 0: stage an attacker blob under an attacker tree entry name
 *         (hostile names like "../x" or ".git/..." are the cases where
 *         upstream validation matters)
 *   op 1: write the staged tree and commit it on refs/heads/main (HEAD)
 *   op 2: write the staged tree and commit it on refs/heads/theirs
 *         (branch auto-created at the base commit), so the two branches
 *         diverge with attacker trees on both sides
 *   op 3: git_merge_analysis_for_ref + git_merge(theirs) with attacker
 *         preference bits (favor normal/ours/theirs/union, renames)
 *   op 4: git_checkout_tree of HEAD / git_checkout_head with attacker
 *         strategy bits
 *   op 5: walk the index written by checkout/merge
 *
 * Each iteration uses its own directory under a shared tmpdir and is
 * removed afterwards, so disk stays bounded across a campaign.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "git2.h"
#include "futils.h"

#include "standalone_driver.h"

#define UNUSED(x) (void)(x)

/* Keep per-iteration attacker volume bounded so that memory and CPU use
 * stay attributable to upstream's parsing/writing choices. */
#define MAX_INPUT_SIZE	(64 * 1024)
#define MAX_NAME_LEN	64
#define MAX_BLOB_LEN	512
#define MAX_OPS		8
#define MAX_ITER_DIR	4096

static char base_tmp[MAX_ITER_DIR];

struct cursor {
	const uint8_t *data;
	size_t size;
};

static int take8(struct cursor *c, unsigned *out)
{
	if (c->size < 1)
		return -1;
	*out = *c->data++;
	c->size--;
	return 0;
}

static int take16(struct cursor *c, size_t *out)
{
	if (c->size < 2)
		return -1;
	*out = (size_t)c->data[0] | ((size_t)c->data[1] << 8);
	c->data += 2;
	c->size -= 2;
	return 0;
}

static int take_bytes(struct cursor *c, const uint8_t **out, size_t len)
{
	if (c->size < len)
		return -1;
	*out = c->data;
	c->data += len;
	c->size -= len;
	return 0;
}

static int take_name(struct cursor *c, char *name)
{
	unsigned len;
	const uint8_t *bytes;

	if (take8(c, &len) < 0)
		return -1;
	len &= (MAX_NAME_LEN - 1);
	if (take_bytes(c, &bytes, len) < 0)
		return -1;
	memcpy(name, bytes, len);
	name[len] = '\0';
	return 0;
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	char tmpl[] = "/tmp/git2mc.XXXXXX";

	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();

	if (mkdtemp(tmpl) != tmpl)
		abort();

	if (strlen(tmpl) + 4 >= MAX_ITER_DIR)
		abort();
	memcpy(base_tmp, tmpl, strlen(tmpl) + 1);

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct cursor c = { data, size };
	git_repository *repo = NULL;
	git_treebuilder *bld = NULL;
	git_reference *head_ref = NULL;
	git_reference *their_ref = NULL;
	git_commit *parent = NULL;
	git_signature *sig = NULL;
	char iter_dir[MAX_ITER_DIR];
	char name[MAX_NAME_LEN + 1];
	unsigned nops, opcode, bits;
	git_oid base_id, tree_id, commit_id;
	size_t i;
	git_tree *tree = NULL;

	if (size > MAX_INPUT_SIZE)
		return 0;

	if (snprintf(iter_dir, sizeof(iter_dir), "%s/it", base_tmp) < 0)
		return 0;

	/* Fresh non-bare fixture: checkout needs a working directory. */
	if (mkdir(iter_dir, 0777) != 0)
		return 0;

	if (git_repository_init(&repo, iter_dir, 0) < 0)
		goto done;

	/* Deterministic base commit so merge/checkout always have history. */
	if (git_signature_now(&sig, "fuzzer", "fuzzer@example.com") < 0 ||
	    git_blob_create_from_buffer(&tree_id, repo, "base\n", 5) < 0 ||
	    git_treebuilder_new(&bld, repo, NULL) < 0 ||
	    git_treebuilder_insert(NULL, bld, "base.txt", &tree_id,
		GIT_FILEMODE_BLOB) < 0 ||
	    git_treebuilder_write(&base_id, bld) < 0 ||
	    git_tree_lookup(&tree, repo, &base_id) < 0 ||
	    git_commit_create(&commit_id, repo, "HEAD", sig, sig,
		NULL, "base", tree, 0, NULL) < 0 ||
	    git_treebuilder_clear(bld) < 0)
		goto done;

	if (take8(&c, &nops) < 0)
		goto done;
	if (nops > MAX_OPS)
		nops = MAX_OPS;

	for (i = 0; i < nops; i++) {
		if (take8(&c, &opcode) < 0)
			break;

		switch (opcode & 7) {
		case 0: { /* stage attacker blob under attacker name */
			const uint8_t *bytes;
			size_t len;

			if (take_name(&c, name) < 0)
				break;
			if (name[0] == '\0')
				break;
			if (take16(&c, &len) < 0)
				break;
			len &= MAX_BLOB_LEN;
			if (take_bytes(&c, &bytes, len) < 0)
				break;
			if (git_blob_create_from_buffer(&tree_id, repo,
				bytes, len) < 0)
				break;
			/* Upstream validates the entry name; hostile names
			 * are expected to be rejected here. */
			(void)git_treebuilder_insert(NULL, bld, name,
				&tree_id, GIT_FILEMODE_BLOB);
			break;
		}
		case 1: /* commit staged tree on main (HEAD) */
		case 2: { /* commit staged tree on theirs (divergence) */
			const char *ref = "HEAD";

			if (git_treebuilder_write(&tree_id, bld) < 0)
				break;
			if (git_treebuilder_clear(bld) < 0)
				break;
			git_tree_free(tree);
		tree = NULL;
		if (git_tree_lookup(&tree, repo, &tree_id) < 0)
			break;
		if ((opcode & 7) == 2) {
				ref = "refs/heads/theirs";
				/* branch at the base commit, then diverge */
				(void)git_reference_create(NULL, repo, ref,
					&base_id, 1, NULL);
				if (git_commit_lookup(&parent, repo,
					&base_id) < 0)
					break;
			} else {
				if (git_repository_head(&head_ref, repo) < 0)
					break;
				if (git_reference_peel(
					(git_object **)&parent, head_ref,
					GIT_OBJECT_COMMIT) < 0)
					break;
				git_reference_free(head_ref);
				head_ref = NULL;
			}
			(void)git_commit_create(&commit_id, repo, ref,
				sig, sig, NULL, "fuzz", tree,
				parent ? 1 : 0,
				(const git_commit **)&parent);
			break;
		}
		case 3: { /* merge theirs into HEAD with attacker prefs */
			git_merge_analysis_t analysis;
			git_merge_preference_t pref =
				GIT_MERGE_PREFERENCE_NONE;
			git_merge_options opts = GIT_MERGE_OPTIONS_INIT;
			git_annotated_commit *their_commit = NULL;

			if (take8(&c, &bits) < 0)
				break;
			opts.file_favor =
				(git_merge_file_favor_t)(bits & 3);
			if (bits & 4)
				opts.flags |= GIT_MERGE_FIND_RENAMES;
			if (git_annotated_commit_lookup(&their_commit,
				repo, &commit_id) < 0)
				break;
			if (git_repository_head(&head_ref, repo) >= 0) {
				(void)git_merge_analysis_for_ref(&analysis,
					&pref, repo, head_ref,
					(const git_annotated_commit **)
					&their_commit, 1);
				(void)git_merge(repo,
					(const git_annotated_commit **)
					&their_commit, 1, &opts, NULL);
				git_reference_free(head_ref);
				head_ref = NULL;
			}
			git_annotated_commit_free(their_commit);
			break;
		}
		case 4: { /* checkout head/tree with attacker strategies */
			git_checkout_options copts =
				GIT_CHECKOUT_OPTIONS_INIT;
			git_object *treeish = NULL;

			if (take8(&c, &bits) < 0)
				break;
			copts.checkout_strategy = (bits & 1)
				? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE;
			if (bits & 2)
				copts.checkout_strategy |=
					GIT_CHECKOUT_REMOVE_UNTRACKED;
			if (bits & 4)
				copts.checkout_strategy |=
					GIT_CHECKOUT_ALLOW_CONFLICTS;
			if (bits & 8)
				copts.checkout_strategy |=
					GIT_CHECKOUT_RECREATE_MISSING;
			if (git_repository_head(&head_ref, repo) < 0)
				break;
			if (bits & 0x10) {
				if (git_reference_peel(&treeish, head_ref,
					GIT_OBJECT_TREE) >= 0)
					(void)git_checkout_tree(repo,
						treeish, &copts);
			} else {
				(void)git_checkout_head(repo, &copts);
			}
			git_object_free(treeish);
			git_reference_free(head_ref);
			head_ref = NULL;
			break;
		}
		case 5: { /* walk the index written by checkout/merge */
			git_index *idx = NULL;
			size_t count, k;

			if (git_repository_index(&idx, repo) < 0)
				break;
			count = git_index_entrycount(idx);
			for (k = 0; k < count; k++) {
				if (git_index_get_byindex(idx, k) == NULL)
					break;
			}
			git_index_free(idx);
			break;
		}
		default:
			break;
		}
	}

done:
	git_reference_free(head_ref);
	git_reference_free(their_ref);
	git_commit_free(parent);
	git_tree_free(tree);
	git_treebuilder_free(bld);
	git_signature_free(sig);
	git_repository_free(repo);

	/* Drop the whole iteration directory: disk stays bounded. */
	git_futils_rmdir_r(iter_dir, NULL, GIT_RMDIR_REMOVE_FILES);

	return 0;
}
