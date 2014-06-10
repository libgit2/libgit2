/*
 * libgit2 "blame" example - shows how to use the blame API
 *
 * Written by the libgit2 contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdio.h>

#include "common.h"

/**
 * This example implements the safe portion of the git's 'pull'
 * command. That is, merging fro upstream, which is the behaviour
 * which does not loose procedence information or reverse history.
 */
int main(int argc, char **argv)
{
	git_repository *repo;
	git_reference *current_branch, *upstream;
	git_buf remote_name = {0};
	git_remote *remote;

	git_threads_init();

	/**
	 * Figure out what the current branch's upstream remote is so
	 * we know from which remote to fetch
	 */
	check_lg2(git_repository_open_ext(&repo, ".", 0, NULL), "failed to open repo", NULL);
	check_lg2(git_repository_head(&current_branch, repo), "failed to lookup current branch", NULL);
	check_lg2(git_branch_remote_name(&remote_name, repo, git_reference_name(current_branch)),
		  "failed to get the reference's upstream", NULL);
	check_lg2(git_remote_load(&remote, repo, remote_name.ptr), "failed to load remote", NULL);
	git_buf_free(&remote_name);
	check_lg2(git_remote_fetch(remote, NULL, NULL), "failed to fetch from upstream", NULL);

	/**
	 * Now that we have the updated data from the remote, look up
	 * our branch's upstream and merge from it.
	 */
	{
		git_merge_head *merge_heads[1];
	
		check_lg2(git_branch_upstream(&upstream, current_branch), "failed to get upstream branch", NULL);
		check_lg2(git_merge_head_from_ref(&merge_heads[0], repo, upstream), "failed to create merge head", NULL);
		check_lg2(git_merge(repo, (const git_merge_head **) merge_heads, 1, NULL, NULL), "failed to merge", NULL);
		git_merge_head_free(merge_heads[1]);
	}

	/**
	 * Once the merge operation succeeds, we need to check whether
	 * there were any conflicts merging
	 */
	{
		git_index *index;
		int has_conflicts;

		check_lg2(git_repository_index(&index, repo), "failed to load index", NULL);
		has_conflicts = git_index_has_conflicts(index);

		git_index_free(index);

		if (has_conflicts) {
			printf("There were conflicts merging. Please resolve them and commit\n");
			git_reference_free(upstream);
			git_reference_free(current_branch);
			git_repository_free(repo);

			return 0;
		}
	}

	/**
	 * If there were no conflicts, then we commit with the message
	 * that was prepared by the merge operation.
	 *
	 * A tool would take this opportunity to spawn the user's
	 * editor and let them change it, but that is outside of our
	 * purpose here.
	 *
	 * 
	 */
	{
		git_index *index;
		git_buf message = {0};
		git_oid commit_id, tree_id;
		git_commit *parents[2];
		git_signature *user;
		git_tree *tree;

		/* Get the contents of the index into the repo as the tree want for the commit */
		check_lg2(git_repository_index(&index, repo), "failed to load index", NULL);
		check_lg2(git_index_write_tree(&tree_id, index), "failed to write tree", NULL);
		git_index_free(index);

		check_lg2(git_signature_default(&user, repo), "failed to get user's ident", NULL);
		check_lg2(git_repository_message(&message, repo), "failed to get message", NULL);

		check_lg2(git_tree_lookup(&tree, repo, &tree_id), "failed to lookup tree", NULL);

		check_lg2(git_commit_lookup(&parents[0], repo, git_reference_target(current_branch)),
			  "failed to lookup first parent", NULL);
		check_lg2(git_commit_lookup(&parents[1], repo, git_reference_target(upstream)),
			  "failed to lookup second parent", NULL);

		check_lg2(git_commit_create(&commit_id, repo, "HEAD", user, user,
					    NULL, message.ptr,
					    tree, 2, (const git_commit **) parents),
			  "failed to create commit", NULL);

		git_tree_free(tree);
		git_signature_free(user);
	}
	
	git_reference_free(upstream);
	git_reference_free(current_branch);
	git_repository_free(repo);

	return 0;

}










