/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "notes.h"

#include "git2.h"
#include "refs.h"

static int find_subtree(git_tree **subtree, const git_oid *root,
			git_repository *repo, const char *target, int *fanout)
{
	int error;
	unsigned int i;
	git_tree *tree;
	const git_tree_entry *entry;

	*subtree = NULL;

	error = git_tree_lookup(&tree, repo, root);
	if (error < GIT_SUCCESS) {
		if (error == GIT_ENOTFOUND)
			return error; /* notes tree doesn't exist yet */
		return git__rethrow(error, "Failed to open notes tree");
	}

	for (i=0; i<git_tree_entrycount(tree); i++) {
		entry = git_tree_entry_byindex(tree, i);

		if (!git__ishex(git_tree_entry_name(entry)))
			continue;

		/*
		 * A notes tree follows a strict byte-based progressive fanout
		 * (i.e. using 2/38, 2/2/36, etc. fanouts, not e.g. 4/36 fanout)
		 */

		if (S_ISDIR(git_tree_entry_attributes(entry))
			&& strlen(git_tree_entry_name(entry)) == 2
			&& !strncmp(git_tree_entry_name(entry), target + *fanout, 2)) {

			/* found matching subtree - unpack and resume lookup */

			git_oid subtree_sha;
			git_oid_cpy(&subtree_sha, git_tree_entry_id(entry));
			git_tree_free(tree);

			*fanout += 2;

			return find_subtree(subtree, &subtree_sha, repo,
					    target, fanout);
		}
	}

	*subtree = tree;
	return GIT_SUCCESS;
}

static int find_blob(git_oid *blob, git_tree *tree, const char *target)
{
	unsigned int i;
	const git_tree_entry *entry;

	for (i=0; i<git_tree_entrycount(tree); i++) {
		entry = git_tree_entry_byindex(tree, i);

		if (!strcmp(git_tree_entry_name(entry), target)) {
			/* found matching note object - return */

			git_oid_cpy(blob, git_tree_entry_id(entry));
			return GIT_SUCCESS;
		}
	}
	return GIT_ENOTFOUND;
}

static int note_write(git_oid *out, git_repository *repo,
		      git_signature *author, git_signature *committer,
		      const char *notes_ref, const char *note,
		      const git_oid *tree_sha, const char *target,
		      int nparents, git_commit **parents)
{
	int error, fanout = 0;
	git_oid oid;
	git_tree *tree = NULL;
	git_tree_entry *entry;
	git_treebuilder *tb;

	/* check for existing notes tree */

	if (tree_sha) {
		error = find_subtree(&tree, tree_sha, repo, target, &fanout);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to lookup subtree");

		error = find_blob(&oid, tree, target + fanout);
		if (error < GIT_SUCCESS && error != GIT_ENOTFOUND) {
			git_tree_free(tree);
			return git__throw(GIT_ENOTFOUND, "Failed to read subtree %s", target);
		}

		if (error == GIT_SUCCESS) {
			git_tree_free(tree);
			return git__throw(GIT_EEXISTS, "Note for `%s` exists already", target);
		}
	}

	/* no matching tree entry - add note object to target tree */

	error = git_treebuilder_create(&tb, tree);
	git_tree_free(tree);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create treebuilder");

	if (!tree_sha)
		/* no notes tree yet - create fanout */
		fanout += 2;

	/* create note object */
	error = git_blob_create_frombuffer(&oid, repo, note, strlen(note));
	if (error < GIT_SUCCESS) {
		git_treebuilder_free(tb);
		return git__rethrow(error, "Failed to create note object");
	}

	error = git_treebuilder_insert(&entry, tb, target + fanout, &oid, 0100644);
	if (error < GIT_SUCCESS) {
		/* libgit2 doesn't support object removal (gc) yet */
		/* we leave an orphaned blob object behind - TODO */

		git_treebuilder_free(tb);
		return git__rethrow(error, "Failed to insert note object");
	}

	if (out)
		git_oid_cpy(out, git_tree_entry_id(entry));

	error = git_treebuilder_write(&oid, repo, tb);
	git_treebuilder_free(tb);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write notes tree");

	if (!tree_sha) {
		/* create fanout subtree */

		char subtree[3];
		strncpy(subtree, target, 2);
		subtree[2] = '\0';

		error = git_treebuilder_create(&tb, NULL);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to create treebuilder");

		error = git_treebuilder_insert(NULL, tb, subtree, &oid, 0040000);
		if (error < GIT_SUCCESS) {
			git_treebuilder_free(tb);
			return git__rethrow(error, "Failed to insert note object");
		}

		error = git_treebuilder_write(&oid, repo, tb);
		git_treebuilder_free(tb);

		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to write notes tree");
	}

	/* create new notes commit */

	error = git_tree_lookup(&tree, repo, &oid);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to open new notes tree");

	error = git_commit_create(&oid, repo, notes_ref, author, committer,
				  NULL, GIT_NOTES_DEFAULT_MSG_ADD,
				  tree, nparents, (const git_commit **) parents);

	git_tree_free(tree);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create new notes commit");

	return GIT_SUCCESS;
}

static int note_lookup(git_note **out, git_repository *repo,
		       const git_oid *tree_sha, const char *target)
{
	int error, fanout = 0;
	git_oid oid;
	git_blob *blob;
	git_tree *tree;
	git_note *note;

	error = find_subtree(&tree, tree_sha, repo, target, &fanout);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup subtree");

	error = find_blob(&oid, tree, target + fanout);
	if (error < GIT_SUCCESS) {
		git_tree_free(tree);
		return git__throw(GIT_ENOTFOUND, "No note found for object %s",
				  target);
	}
	git_tree_free(tree);

	error = git_blob_lookup(&blob, repo, &oid);
	if (error < GIT_SUCCESS)
		return git__throw(GIT_ERROR, "Failed to lookup note object");

	note = git__malloc(sizeof(git_note));
	if (note == NULL) {
		git_blob_free(blob);
		return GIT_ENOMEM;
	}

	git_oid_cpy(&note->oid, &oid);
	note->message = git__strdup(git_blob_rawcontent(blob));
	if (note->message == NULL)
		error = GIT_ENOMEM;

	*out = note;

	git_blob_free(blob);
	return error;
}

static int note_remove(git_repository *repo,
		       git_signature *author, git_signature *committer,
		       const char *notes_ref, const git_oid *tree_sha,
		       const char *target, int nparents, git_commit **parents)
{
	int error, fanout = 0;
	git_oid oid;
	git_tree *tree;
	git_treebuilder *tb;

	error = find_subtree(&tree, tree_sha, repo, target, &fanout);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup subtree");

	error = find_blob(&oid, tree, target + fanout);
	if (error < GIT_SUCCESS) {
		git_tree_free(tree);
		return git__throw(GIT_ENOTFOUND, "No note found for object %s",
				  target);
	}

	error = git_treebuilder_create(&tb, tree);
	git_tree_free(tree);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create treebuilder");

	error = git_treebuilder_remove(tb, target + fanout);
	if (error < GIT_SUCCESS) {
		git_treebuilder_free(tb);
		return git__rethrow(error, "Failed to remove entry from notes tree");
	}

	error = git_treebuilder_write(&oid, repo, tb);
	git_treebuilder_free(tb);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to write notes tree");

	/* create new notes commit */

	error = git_tree_lookup(&tree, repo, &oid);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to open new notes tree");

	error = git_commit_create(&oid, repo, notes_ref, author, committer,
				  NULL, GIT_NOTES_DEFAULT_MSG_RM,
				  tree, nparents, (const git_commit **) parents);

	git_tree_free(tree);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to create new notes commit");

	return error;
}

int git_note_read(git_note **out, git_repository *repo,
		  const char *notes_ref, const git_oid *oid)
{
	int error;
	char *target;
	git_reference *ref;
	git_commit *commit;
	const git_oid *sha;

	*out = NULL;

	if (!notes_ref)
		notes_ref = GIT_NOTES_DEFAULT_REF;

	error = git_reference_lookup(&ref, repo, notes_ref);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference `%s`", notes_ref);

	assert(git_reference_type(ref) == GIT_REF_OID);

	sha = git_reference_oid(ref);
	error = git_commit_lookup(&commit, repo, sha);

	git_reference_free(ref);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find notes commit object");

	sha = git_commit_tree_oid(commit);
	git_commit_free(commit);

	target = git_oid_allocfmt(oid);
	if (target == NULL)
		return GIT_ENOMEM;

	error = note_lookup(out, repo, sha, target);

	git__free(target);
	return error == GIT_SUCCESS ? GIT_SUCCESS :
		git__rethrow(error, "Failed to read note");
}

int git_note_create(git_oid *out, git_repository *repo,
		    git_signature *author, git_signature *committer,
		    const char *notes_ref, const git_oid *oid,
		     const char *note)
{
	int error, nparents = 0;
	char *target;
	git_oid sha;
	git_commit *commit = NULL;
	git_reference *ref;

	if (!notes_ref)
		notes_ref = GIT_NOTES_DEFAULT_REF;

	error = git_reference_lookup(&ref, repo, notes_ref);
	if (error < GIT_SUCCESS && error != GIT_ENOTFOUND)
		return git__rethrow(error, "Failed to lookup reference `%s`", notes_ref);

	if (error == GIT_SUCCESS) {
		assert(git_reference_type(ref) == GIT_REF_OID);

		/* lookup existing notes tree oid */

		git_oid_cpy(&sha, git_reference_oid(ref));
		git_reference_free(ref);

		error = git_commit_lookup(&commit, repo, &sha);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to find notes commit object");

		git_oid_cpy(&sha, git_commit_tree_oid(commit));
		nparents++;
	}

	target = git_oid_allocfmt(oid);
	if (target == NULL)
		return GIT_ENOMEM;

	error = note_write(out, repo, author, committer, notes_ref,
			   note, nparents ? &sha : NULL, target,
			   nparents, &commit);

	git__free(target);
	git_commit_free(commit);
	return error == GIT_SUCCESS ? GIT_SUCCESS :
		git__rethrow(error, "Failed to write note");
}

int git_note_remove(git_repository *repo, const char *notes_ref,
		    git_signature *author, git_signature *committer,
		    const git_oid *oid)
{
	int error;
	char *target;
	git_oid sha;
	git_commit *commit;
	git_reference *ref;

	if (!notes_ref)
		notes_ref = GIT_NOTES_DEFAULT_REF;

	error = git_reference_lookup(&ref, repo, notes_ref);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to lookup reference `%s`", notes_ref);

	assert(git_reference_type(ref) == GIT_REF_OID);

	git_oid_cpy(&sha, git_reference_oid(ref));
	git_reference_free(ref);

	error = git_commit_lookup(&commit, repo, &sha);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find notes commit object");

	git_oid_cpy(&sha, git_commit_tree_oid(commit));

	target = git_oid_allocfmt(oid);
	if (target == NULL)
		return GIT_ENOMEM;

	error = note_remove(repo, author, committer, notes_ref,
			    &sha, target, 1, &commit);

	git__free(target);
	git_commit_free(commit);
	return error == GIT_SUCCESS ? GIT_SUCCESS :
		git__rethrow(error, "Failed to read note");
}

const char * git_note_message(git_note *note)
{
	assert(note);
	return note->message;
}

const git_oid * git_note_oid(git_note *note)
{
	assert(note);
	return &note->oid;
}

void git_note_free(git_note *note)
{
	if (note == NULL)
		return;

	git__free(note->message);
	git__free(note);
}
