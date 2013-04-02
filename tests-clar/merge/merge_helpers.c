#include "clar_libgit2.h"
#include "buffer.h"
#include "refs.h"
#include "tree.h"
#include "merge_helpers.h"
#include "merge.h"
#include "git2/merge.h"

int merge_trees_from_branches(
	git_index **index, git_repository *repo,
	const char *ours_name, const char *theirs_name,
	git_merge_tree_opts *opts)
{
	git_commit *our_commit, *their_commit, *ancestor_commit = NULL;
	git_tree *our_tree, *their_tree, *ancestor_tree = NULL;
	git_oid our_oid, their_oid, ancestor_oid;
	git_buf branch_buf = GIT_BUF_INIT;
	int error;

	git_buf_printf(&branch_buf, "%s%s", GIT_REFS_HEADS_DIR, ours_name);
	cl_git_pass(git_reference_name_to_id(&our_oid, repo, branch_buf.ptr));
	cl_git_pass(git_commit_lookup(&our_commit, repo, &our_oid));

	git_buf_clear(&branch_buf);
	git_buf_printf(&branch_buf, "%s%s", GIT_REFS_HEADS_DIR, theirs_name);
	cl_git_pass(git_reference_name_to_id(&their_oid, repo, branch_buf.ptr));
	cl_git_pass(git_commit_lookup(&their_commit, repo, &their_oid));

	error = git_merge_base(&ancestor_oid, repo, git_commit_id(our_commit), git_commit_id(their_commit));

	if (error != GIT_ENOTFOUND) {
		cl_git_pass(error);

		cl_git_pass(git_commit_lookup(&ancestor_commit, repo, &ancestor_oid));
		cl_git_pass(git_commit_tree(&ancestor_tree, ancestor_commit));
	}

	cl_git_pass(git_commit_tree(&our_tree, our_commit));
	cl_git_pass(git_commit_tree(&their_tree, their_commit));

	cl_git_pass(git_merge_trees(index, repo, ancestor_tree, our_tree, their_tree, opts));

	git_buf_free(&branch_buf);
	git_tree_free(our_tree);
	git_tree_free(their_tree);
	git_tree_free(ancestor_tree);
	git_commit_free(our_commit);
	git_commit_free(their_commit);
	git_commit_free(ancestor_commit);

	return 0;
}

static int index_entry_eq_merge_index_entry(const struct merge_index_entry *expected, const git_index_entry *actual)
{
	git_oid expected_oid;
    bool test_oid;

	if (strlen(expected->oid_str) != 0) {
		cl_git_pass(git_oid_fromstr(&expected_oid, expected->oid_str));
		test_oid = 1;
	} else
		test_oid = 0;
	
	if (actual->mode != expected->mode ||
		(test_oid && git_oid_cmp(&actual->oid, &expected_oid) != 0) ||
		git_index_entry_stage(actual) != expected->stage)
		return 0;
	
	if (actual->mode == 0 && (actual->path != NULL || strlen(expected->path) > 0))
		return 0;

	if (actual->mode != 0 && (strcmp(actual->path, expected->path) != 0))
		return 0;
	
	return 1;
}

static int name_entry_eq(const char *expected, const char *actual)
{
	if (strlen(expected) == 0)
		return (actual == NULL) ? 1 : 0;
	
	return (strcmp(expected, actual) == 0) ? 1 : 0;
}

static int index_conflict_data_eq_merge_diff(const struct merge_index_conflict_data *expected, git_merge_diff *actual)
{
	if (!index_entry_eq_merge_index_entry((const struct merge_index_entry *)&expected->ancestor, &actual->ancestor_entry) ||
		!index_entry_eq_merge_index_entry((const struct merge_index_entry *)&expected->ours, &actual->our_entry) ||
		!index_entry_eq_merge_index_entry((const struct merge_index_entry *)&expected->theirs, &actual->their_entry))
		return 0;
	
	if (expected->ours.status != actual->our_status ||
		expected->theirs.status != actual->their_status)
		return 0;
    
	return 1;
}

int merge_test_merge_conflicts(git_vector *conflicts, const struct merge_index_conflict_data expected[], size_t expected_len)
{
	git_merge_diff *actual;
	size_t i;
	
	if (conflicts->length != expected_len)
		return 0;

	for (i = 0; i < expected_len; i++) {
		actual = conflicts->contents[i];
		
		if (!index_conflict_data_eq_merge_diff(&expected[i], actual))
			return 0;
	}

    return 1;
}

int merge_test_index(git_index *index, const struct merge_index_entry expected[], size_t expected_len)
{
    size_t i;
    const git_index_entry *index_entry;
	
	/*
	dump_index_entries(&index->entries);
	*/

    if (git_index_entrycount(index) != expected_len)
        return 0;
    
    for (i = 0; i < expected_len; i++) {
        if ((index_entry = git_index_get_byindex(index, i)) == NULL)
            return 0;
		
		if (!index_entry_eq_merge_index_entry(&expected[i], index_entry))
			return 0;
    }
    
    return 1;
}

int merge_test_reuc(git_index *index, const struct merge_reuc_entry expected[], size_t expected_len)
{
    size_t i;
	const git_index_reuc_entry *reuc_entry;
    git_oid expected_oid;
	
	/*
	dump_reuc(index);
	*/
	
    if (git_index_reuc_entrycount(index) != expected_len)
        return 0;
    
    for (i = 0; i < expected_len; i++) {
        if ((reuc_entry = git_index_reuc_get_byindex(index, i)) == NULL)
            return 0;

		if (strcmp(reuc_entry->path, expected[i].path) != 0 ||
			reuc_entry->mode[0] != expected[i].ancestor_mode ||
			reuc_entry->mode[1] != expected[i].our_mode ||
			reuc_entry->mode[2] != expected[i].their_mode)
			return 0;

		if (expected[i].ancestor_mode > 0) {
			cl_git_pass(git_oid_fromstr(&expected_oid, expected[i].ancestor_oid_str));

			if (git_oid_cmp(&reuc_entry->oid[0], &expected_oid) != 0)
				return 0;
		}

		if (expected[i].our_mode > 0) {
			cl_git_pass(git_oid_fromstr(&expected_oid, expected[i].our_oid_str));

			if (git_oid_cmp(&reuc_entry->oid[1], &expected_oid) != 0)
				return 0;
		}

		if (expected[i].their_mode > 0) {
			cl_git_pass(git_oid_fromstr(&expected_oid, expected[i].their_oid_str));

			if (git_oid_cmp(&reuc_entry->oid[2], &expected_oid) != 0)
				return 0;
		}
    }
    
    return 1;
}

int dircount(void *payload, git_buf *pathbuf)
{
	int *entries = payload;
	size_t len = git_buf_len(pathbuf);
	
	if (len < 5 || strcmp(pathbuf->ptr + (git_buf_len(pathbuf) - 5), "/.git") != 0)
		(*entries)++;
	
	return 0;
}

int merge_test_workdir(git_repository *repo, const struct merge_index_entry expected[], size_t expected_len)
{
	size_t actual_len = 0, i;
	git_oid actual_oid, expected_oid;
	git_buf wd = GIT_BUF_INIT;
	
	git_buf_puts(&wd, repo->workdir);	
	git_path_direach(&wd, dircount, &actual_len);
	
	if (actual_len != expected_len)
		return 0;
		
	for (i = 0; i < expected_len; i++) {
		git_blob_create_fromworkdir(&actual_oid, repo, expected[i].path);
		git_oid_fromstr(&expected_oid, expected[i].oid_str);
		
		if (git_oid_cmp(&actual_oid, &expected_oid) != 0)
			return 0;
	}
	
	git_buf_free(&wd);
	
	return 1;
}
