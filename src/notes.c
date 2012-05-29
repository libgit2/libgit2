/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "notes.h"

#include "git2.h"
#include "refs.h"
#include "config.h"
#include "iterator.h"

static int find_subtree_in_current_level(git_tree **out, git_repository *repo, git_tree *parent, const char *annotated_object_sha, int fanout)
{
	unsigned int i;
	const git_tree_entry *entry;

	*out = NULL;
	
	if (parent == NULL)
		return GIT_ENOTFOUND;

	for (i = 0; i < git_tree_entrycount(parent); i++) {
		entry = git_tree_entry_byindex(parent, i);

		if (!git__ishex(git_tree_entry_name(entry)))
			continue;

		if (S_ISDIR(git_tree_entry_attributes(entry)) 
			&& strlen(git_tree_entry_name(entry)) == 2 
			&& !strncmp(git_tree_entry_name(entry), annotated_object_sha + fanout, 2))
			return git_tree_lookup(out, repo, git_tree_entry_id(entry));

		/* Not a DIR, so do we have an already existing blob */
		if (!strcmp(git_tree_entry_name(entry), annotated_object_sha + fanout))				
			return GIT_EEXISTS;
	}

	return GIT_ENOTFOUND;
}

static int find_subtree_r(git_tree **out, const git_oid *root,
			git_repository *repo, const char *target, int *fanout)
{
	int error;
	git_tree *tree, *subtree;

	*out = NULL;

	error = git_tree_lookup(&tree, repo, root);
	if (error < 0)
		goto cleanup;

	error = find_subtree_in_current_level(&subtree, repo, tree, target, *fanout);
	if (error == GIT_EEXISTS)
	{
		*out = tree;
		error = 0;
		goto cleanup;
	}

	if (error < 0)
		goto cleanup;

	*fanout += 2;
	error = find_subtree_r(out, git_object_id((const git_object *)subtree), repo, target, fanout);
	git_tree_free(tree);

cleanup:
	git_tree_free(subtree);
	return error;
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
			return 0;
		}
	}
	return GIT_ENOTFOUND;
}

static int tree_write(git_tree **out, git_repository *repo, git_tree *source_tree, const git_oid *object_oid, const char *treeentry_name, unsigned int attributes)
{
	int error;
	git_treebuilder *tb = NULL;
	git_tree_entry *entry;
	git_oid tree_oid;

	error = git_treebuilder_create(&tb, source_tree);
	if (error < 0)
		goto cleanup;

	error = git_treebuilder_insert(&entry, tb, treeentry_name, object_oid, attributes);
	if (error < 0)
		goto cleanup;

	error = git_treebuilder_write(&tree_oid, repo, tb);
	if (error < 0)
		goto cleanup;

	error = git_tree_lookup(out, repo, &tree_oid);

cleanup:
	git_treebuilder_free(tb);
	return error;
}

static int insert_note_in_tree(git_tree **out, git_repository *repo, git_tree *parent, git_oid *note_oid, const char *annotated_object_sha, int fanout)
{
	int error = -1;	
	git_tree *subtree;
	char subtree_name[3];

	error = find_subtree_in_current_level(&subtree, repo, parent, annotated_object_sha, fanout);

	if (error == GIT_EEXISTS) {
		giterr_set(GITERR_REPOSITORY, "Note for '%s' exists already", annotated_object_sha);
		goto cleanup;
	}

	if (error == GIT_ENOTFOUND) {
		/* No existing fanout at this level, insert in place */
		error = tree_write(out, repo, parent, note_oid, annotated_object_sha + fanout, 0100644);
		goto cleanup;
	}

	if (error < 0)
		goto cleanup;

	/* An existing fanout has been found, let's dig deeper */
	error = insert_note_in_tree(out, repo, subtree, note_oid, annotated_object_sha, fanout + 2);
	if (error < 0)
		goto cleanup;

	strncpy(subtree_name, annotated_object_sha + fanout, 2);
	subtree_name[2] = '\0';

	error = tree_write(out, repo, parent, git_object_id((const git_object *)(*out)), subtree_name, 0040000);

cleanup:
	git_tree_free(subtree);
	return error;
}

static int note_write(git_oid *out, git_repository *repo,
		      git_signature *author, git_signature *committer,
		      const char *notes_ref, const char *note,
		      git_tree *commit_tree, const char *target,
		      git_commit **parents)
{
	int error;
	git_oid oid;
	git_tree *tree = NULL;
	
	// TODO: should we apply filters?
	/* create note object */
	if ((error = git_blob_create_frombuffer(&oid, repo, note, strlen(note))) < 0)
		goto cleanup;

	if ((error = manipulate_note_in_tree_r(&tree, repo, commit_tree, &oid, target, 0, insert_note_in_tree_eexists_cb, insert_note_in_tree_enotfound_cb)) < 0)
		goto cleanup;

	if (out)
		git_oid_cpy(out, &oid);

	error = git_commit_create(&oid, repo, notes_ref, author, committer,
				  NULL, GIT_NOTES_DEFAULT_MSG_ADD,
				  tree, *parents == NULL ? 0 : 1, (const git_commit **) parents);

cleanup:
	git_tree_free(tree);
	return error;
}

static int note_lookup(git_note **out, git_repository *repo,
		       const git_oid *tree_sha, const char *target)
{
	int error, fanout = 0;
	git_oid oid;
	git_blob *blob;
	git_tree *tree;
	git_note *note;

	error = find_subtree_r(&tree, tree_sha, repo, target, &fanout);
	if (error < 0)
		return error;

	error = find_blob(&oid, tree, target + fanout);
	git_tree_free(tree);
	if (error < 0)
		return error;

	error = git_blob_lookup(&blob, repo, &oid);
	if (error < 0)
		return error;

	note = git__malloc(sizeof(git_note));
	GITERR_CHECK_ALLOC(note);

	git_oid_cpy(&note->oid, &oid);
	note->message = git__strdup(git_blob_rawcontent(blob));
	GITERR_CHECK_ALLOC(note->message);

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

	error = find_subtree_r(&tree, tree_sha, repo, target, &fanout);
	if (error < 0)
		return error;

	error = find_blob(&oid, tree, target + fanout);
	if (!error)
		error = git_treebuilder_create(&tb, tree);

	git_tree_free(tree);
	if (error < 0)
		return error;

	error = git_treebuilder_remove(tb, target + fanout);
	if (!error)
		error = git_treebuilder_write(&oid, repo, tb);

	git_treebuilder_free(tb);
	if (error < 0)
		return error;

	/* create new notes commit */

	error = git_tree_lookup(&tree, repo, &oid);
	if (error < 0)
		return error;

	error = git_commit_create(&oid, repo, notes_ref, author, committer,
				  NULL, GIT_NOTES_DEFAULT_MSG_RM,
				  tree, nparents, (const git_commit **) parents);

	git_tree_free(tree);

	return error;
}

static int note_get_default_ref(const char **out, git_repository *repo)
{
	int ret;
	git_config *cfg;

	*out = NULL;

	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	ret = git_config_get_string(out, cfg, "core.notesRef");
	if (ret == GIT_ENOTFOUND) {
		*out = GIT_NOTES_DEFAULT_REF;
		return 0;
	}

	return ret;
}

static int normalize_namespace(const char **notes_ref, git_repository *repo)
{
	if (*notes_ref)
		return 0;

	return note_get_default_ref(notes_ref, repo);
}

static int retrieve_note_tree_oid(git_oid *tree_oid_out, git_repository *repo, const char *notes_ref)
{
	int error = -1;
	git_commit *commit = NULL;
	git_oid oid;

	if ((error = git_reference_name_to_oid(&oid, repo, notes_ref)) < 0)
		goto cleanup;

	if (git_commit_lookup(&commit, repo, &oid) < 0)
		goto cleanup;

	git_oid_cpy(tree_oid_out, git_commit_tree_oid(commit));

	error = 0;

cleanup:
	git_commit_free(commit);
	return error;
}

int git_note_read(git_note **out, git_repository *repo,
		  const char *notes_ref, const git_oid *oid)
{
	int error;
	char *target;
	git_oid sha;

	*out = NULL;

	if (normalize_namespace(&notes_ref, repo) < 0)
		return -1;

	if ((error = retrieve_note_tree_oid(&sha, repo, notes_ref)) < 0)
		return error;

	target = git_oid_allocfmt(oid);
	GITERR_CHECK_ALLOC(target);

	error = note_lookup(out, repo, &sha, target);

	git__free(target);
	return error;
}

int git_note_create(
	git_oid *out, git_repository *repo,
	git_signature *author, git_signature *committer,
	const char *notes_ref, const git_oid *oid,
	const char *note)
{
	int error;
	char *target = NULL;
	git_commit *commit = NULL;
	git_reference *ref;
	git_tree *tree = NULL;

	target = git_oid_allocfmt(oid);
	GITERR_CHECK_ALLOC(target);

	if (normalize_namespace(&notes_ref, repo) < 0)
		return -1;

	error = git_reference_lookup(&ref, repo, notes_ref);
	if (error < 0 && error != GIT_ENOTFOUND)
		goto cleanup;

	if (!error) {
		assert(git_reference_type(ref) == GIT_REF_OID);
	
		error = git_commit_lookup(&commit, repo, git_reference_oid(ref));
		if (error < 0)
			goto cleanup;

		error = git_commit_tree(&tree, commit);
		if (error < 0)
			goto cleanup;
	}

	error = note_write(out, repo, author, committer, notes_ref,
			   note, tree, target, &commit);

cleanup:
	git__free(target);
	git_reference_free(ref);
	git_commit_free(commit);
	git_tree_free(tree);
	return error;
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

	if (normalize_namespace(&notes_ref, repo) < 0)
		return -1;

	error = git_reference_lookup(&ref, repo, notes_ref);
	if (error < 0)
		return error;

	assert(git_reference_type(ref) == GIT_REF_OID);

	git_oid_cpy(&sha, git_reference_oid(ref));
	git_reference_free(ref);

	error = git_commit_lookup(&commit, repo, &sha);
	if (error < 0)
		return error;

	git_oid_cpy(&sha, git_commit_tree_oid(commit));

	target = git_oid_allocfmt(oid);
	GITERR_CHECK_ALLOC(target);

	error = note_remove(repo, author, committer, notes_ref,
			    &sha, target, 1, &commit);

	git__free(target);
	git_commit_free(commit);
	return error;
}

int git_note_default_ref(const char **out, git_repository *repo)
{
	assert(repo);
	return note_get_default_ref(out, repo);
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

static int process_entry_path(
	const char* entry_path,
	const git_oid *note_oid,
	int (*note_cb)(git_note_data *note_data, void *payload),
	void *payload)
{
	int i = 0, j = 0, error = -1, len;
	git_buf buf = GIT_BUF_INIT;
	git_note_data note_data;

	if (git_buf_puts(&buf, entry_path) < 0)
		goto cleanup;
	
	len = git_buf_len(&buf);

	while (i < len) {
		if (buf.ptr[i] == '/') {
			i++;
			continue;
		}
		
		if (git__fromhex(buf.ptr[i]) < 0) {
			/* This is not a note entry */
			error = 0;
			goto cleanup;
		}

		if (i != j)
			buf.ptr[j] = buf.ptr[i];

		i++;
		j++;
	}

	buf.ptr[j] = '\0';
	buf.size = j;

	if (j != GIT_OID_HEXSZ) {
		/* This is not a note entry */
		error = 0;
		goto cleanup;
	}

	if (git_oid_fromstr(&note_data.annotated_object_oid, buf.ptr) < 0)
		return -1;

	git_oid_cpy(&note_data.blob_oid, note_oid);

	error = note_cb(&note_data, payload);

cleanup:
	git_buf_free(&buf);
	return error;
}

int git_note_foreach(
	git_repository *repo,
	const char *notes_ref,
	int (*note_cb)(git_note_data *note_data, void *payload),
	void *payload)
{
	int error = -1;
	git_oid tree_oid;
	git_iterator *iter = NULL;
	git_tree *tree = NULL;
	const git_index_entry *item;

	if (normalize_namespace(&notes_ref, repo) < 0)
		return -1;

	if ((error = retrieve_note_tree_oid(&tree_oid, repo, notes_ref)) < 0)
		goto cleanup;

	if (git_tree_lookup(&tree, repo, &tree_oid) < 0)
		goto cleanup;

	if (git_iterator_for_tree(&iter, repo, tree) < 0)
		goto cleanup;

	if (git_iterator_current(iter, &item) < 0)
		goto cleanup;

	while (item) {
		if (process_entry_path(item->path, &item->oid, note_cb, payload) < 0)
			goto cleanup;

		if (git_iterator_advance(iter, &item) < 0)
			goto cleanup;
	}

	error = 0;

cleanup:
	git_iterator_free(iter);
	git_tree_free(tree);
	return error;
}
