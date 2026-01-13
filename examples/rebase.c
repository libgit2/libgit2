/*
 * libgit2 "rebase" example - shows how to use the rebase API
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

#include "common.h"
#include <sys/stat.h>
#include <unistd.h>

#define REPO_PATH "test-repo"
#define CLONE_PATH "test-repo-clone"

static void check_error(int error_code, const char *action)
{
	if (error_code < 0) {
		const git_error *e = git_error_last();
		fprintf(stderr, "Error %d/%d: %s (%s)\n", error_code, e->klass, action, e->message);
		exit(1);
	}
}

static void create_file(const char *repo_path, const char *filename, const char *content)
{
	char filepath[1024];
	FILE *file;

	snprintf(filepath, sizeof(filepath), "%s/%s", repo_path, filename);
	file = fopen(filepath, "w");
	if (!file) {
		fprintf(stderr, "Failed to create file: %s\n", filepath);
		exit(1);
	}
	fprintf(file, "%s", content);
	fclose(file);
}

/* Global counter for timestamps to ensure consistent ordering */
static int commit_timestamp = 1700000000;  /* Base timestamp: Nov 14, 2023 */

static git_oid commit_file(git_repository *repo, const char *filename, const char *content, const char *message)
{
	git_oid tree_id, commit_id;
	git_tree *tree;
	git_index *index;
	git_signature *sig;
	git_reference *head_ref;
	git_commit *parent = NULL;

	/* Create or update the file */
	const char *workdir = git_repository_workdir(repo);
	create_file(workdir, filename, content);

	/* Add file to index */
	check_error(git_repository_index(&index, repo), "Failed to get index");
	check_error(git_index_add_bypath(index, filename), "Failed to add file to index");
	check_error(git_index_write(index), "Failed to write index");

	/* Write the index as a tree */
	check_error(git_index_write_tree(&tree_id, index), "Failed to write tree");
	check_error(git_tree_lookup(&tree, repo, &tree_id), "Failed to lookup tree");

	/* Create signature with hardcoded timestamp */
	check_error(git_signature_new(&sig, "Test User", "test@example.com",
		commit_timestamp++, 0), "Failed to create signature");

	/* Get parent commit if exists */
	if (git_repository_head(&head_ref, repo) == 0) {
		git_object *head_obj;
		check_error(git_reference_peel(&head_obj, head_ref, GIT_OBJECT_COMMIT), "Failed to peel HEAD");
		parent = (git_commit *)head_obj;
		git_reference_free(head_ref);
	}

	/* Create commit */
	if (parent) {
		const git_commit *parents[] = { parent };
		check_error(git_commit_create(
			&commit_id, repo, "HEAD", sig, sig, NULL, message, tree, 1, parents),
			"Failed to create commit");
		git_commit_free(parent);
	} else {
		check_error(git_commit_create(
			&commit_id, repo, "HEAD", sig, sig, NULL, message, tree, 0, NULL),
			"Failed to create initial commit");
	}

	git_tree_free(tree);
	git_signature_free(sig);
	git_index_free(index);

	return commit_id;
}

static void display_history(git_repository *repo, const char *title, int max_commits)
{
	git_revwalk *walker;
	git_oid oid;
	int count = 0;

	printf("\n%s:\n", title);
	printf("----------------------------------------\n");

	/* Create revision walker */
	check_error(git_revwalk_new(&walker, repo), "Failed to create revwalk");
	git_revwalk_push_head(walker);
	git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

	/* Walk through commits */
	while (git_revwalk_next(&oid, walker) == 0 && count < max_commits) {
		git_commit *commit;
		const char *message;
		char oid_str[GIT_OID_MAX_HEXSIZE + 1];
		char msg_first_line[256];
		const char *newline;
		size_t len;

		check_error(git_commit_lookup(&commit, repo, &oid), "Failed to lookup commit");
		message = git_commit_message(commit);

		/* Format OID as string */
		git_oid_tostr(oid_str, sizeof(oid_str), &oid);

		/* Print commit info - truncate message at newline */
		newline = strchr(message, '\n');
		if (newline) {
			len = newline - message;
			if (len > 255) len = 255;
			strncpy(msg_first_line, message, len);
			msg_first_line[len] = '\0';
		} else {
			strncpy(msg_first_line, message, 255);
			msg_first_line[255] = '\0';
		}

		printf("  %.7s  %s\n", oid_str, msg_first_line);

		git_commit_free(commit);
		count++;
	}

	git_revwalk_free(walker);
	printf("----------------------------------------\n");
}

static void create_initial_repository(const char *path)
{
	git_repository *repo = NULL;
	char cmd[256];

	printf("Creating repository at %s...\n", path);

	/* Remove existing repository if present */
	snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
	system(cmd);

	/* Initialize repository */
	check_error(git_repository_init(&repo, path, 0), "Failed to initialize repository");

	/* Create initial commits */
	printf("Creating initial commits...\n");
	commit_file(repo, "README.md", "# Test Repository\n\nThis is a test repository for demonstrating rebasing.\n", "Initial commit");
	commit_file(repo, "file1.txt", "Content of file 1\nLine 2\nLine 3\n", "Add file1.txt");
	commit_file(repo, "file2.txt", "Content of file 2\nOriginal content\n", "Add file2.txt");

	/* Display initial history */
	display_history(repo, "Initial repository history", 10);

	git_repository_free(repo);
}

static void clone_repository(const char *source_path, const char *dest_path)
{
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *cloned_repo = NULL;
	char cmd[256];

	printf("Cloning repository from %s to %s...\n", source_path, dest_path);

	/* Remove existing clone if present */
	snprintf(cmd, sizeof(cmd), "rm -rf %s", dest_path);
	system(cmd);

	/* Clone the repository */
	check_error(git_clone(&cloned_repo, source_path, dest_path, &clone_opts), "Failed to clone repository");

	git_repository_free(cloned_repo);
}

static void create_divergent_commits(const char *repo1_path, const char *repo2_path)
{
	git_repository *repo1, *repo2;

	printf("\n=== Creating Divergent Commits ===\n");

	/* Open repositories */
	check_error(git_repository_open(&repo1, repo1_path), "Failed to open repository 1");
	check_error(git_repository_open(&repo2, repo2_path), "Failed to open repository 2");

	/* Create commits in repo1 */
	printf("\nCreating commits in original repository (%s)...\n", repo1_path);
	/* This will conflict on line 2 only */
	commit_file(repo1, "file1.txt", "Content of file 1\nLine 2 changed in repo1\nLine 3\nNew line 4 added in repo1\n",
		"Modify file1.txt in repo1");
	commit_file(repo1, "file3.txt", "New file 3 from repo1\n", "Add file3.txt in repo1");
	/* This will conflict on the second line */
	commit_file(repo1, "file2.txt", "Content of file 2\nModified by repo1\nExtra content from repo1\n",
		"Update file2.txt in repo1");

	/* Display repo1 history */
	display_history(repo1, "Original repository history after divergent commits", 10);

	/* Create commits in repo2 (clone) */
	printf("\nCreating commits in cloned repository (%s)...\n", repo2_path);
	/* This will conflict on line 2 only */
	commit_file(repo2, "file1.txt", "Content of file 1\nLine 2 modified in repo2\nLine 3\nLine 4 from repo2\n",
		"Change file1.txt in repo2");
	commit_file(repo2, "file4.txt", "New file 4 from repo2\n", "Add file4.txt in repo2");
	/* This will conflict on the second line */
	commit_file(repo2, "file2.txt", "Content of file 2\nChanged by repo2\nDifferent ending\n",
		"Modify file2.txt differently in repo2");

	/* Display repo2 history */
	display_history(repo2, "Cloned repository history after divergent commits", 10);

	git_repository_free(repo1);
	git_repository_free(repo2);
}

static void fetch_from_upstream(git_repository *repo, const char *upstream_path)
{
	git_remote *remote;
	git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

	printf("Fetching from upstream...\n");

	/* Create a remote pointing to the upstream */
	if (git_remote_lookup(&remote, repo, "upstream") != 0) {
		check_error(git_remote_create(&remote, repo, "upstream", upstream_path),
			"Failed to create upstream remote");
	}

	/* Fetch from upstream */
	check_error(git_remote_fetch(remote, NULL, &fetch_opts, NULL), "Failed to fetch from upstream");

	git_remote_free(remote);
}


static char* read_blob_content(git_repository *repo, const git_oid *oid)
{
	git_blob *blob;
	char *content;
	size_t size;

	if (git_blob_lookup(&blob, repo, oid) != 0) {
		return strdup("(unable to read content)");
	}

	size = git_blob_rawsize(blob);
	content = malloc(size + 1);
	if (content) {
		memcpy(content, git_blob_rawcontent(blob), size);
		content[size] = '\0';
	}

	git_blob_free(blob);
	return content;
}

/* Split content into lines for comparison */
static char** split_lines(const char *content, int *line_count)
{
	int capacity = 16;
	int count = 0;
	char **lines = malloc(capacity * sizeof(char*));
	const char *start = content;
	const char *end;
	int len;

	while (*start) {
		end = strchr(start, '\n');
		if (!end) {
			end = start + strlen(start);
		}

		len = end - start;
		if (count >= capacity) {
			capacity *= 2;
			lines = realloc(lines, capacity * sizeof(char*));
		}

		lines[count] = malloc(len + 2);
		memcpy(lines[count], start, len);
		if (*end == '\n') {
			lines[count][len] = '\n';
			lines[count][len + 1] = '\0';
		} else {
			lines[count][len] = '\0';
		}
		count++;

		if (*end == '\n') {
			start = end + 1;
		} else {
			break;
		}
	}

	*line_count = count;
	return lines;
}

static void free_lines(char **lines, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		free(lines[i]);
	}
	free(lines);
}

static void write_line_by_line_merge(FILE *file, const char *ours_content, const char *theirs_content)
{
	int ours_count, theirs_count;
	char **ours_lines = split_lines(ours_content, &ours_count);
	char **theirs_lines = split_lines(theirs_content, &theirs_count);
	int max_lines = ours_count > theirs_count ? ours_count : theirs_count;
	int in_conflict = 0;
	int conflict_start = -1;
	int i, j;
	const char *ours_line;
	const char *theirs_line;
	int same;

	for (i = 0; i < max_lines; i++) {
		ours_line = i < ours_count ? ours_lines[i] : NULL;
		theirs_line = i < theirs_count ? theirs_lines[i] : NULL;

		/* Check if lines are the same */
		same = 0;
		if (ours_line && theirs_line) {
			same = (strcmp(ours_line, theirs_line) == 0);
		} else {
			same = (ours_line == NULL && theirs_line == NULL);
		}

		if (same) {
			/* Lines match - close any open conflict and write the line */
			if (in_conflict) {
				/* Write all accumulated conflict lines */
				fprintf(file, "<<<<<<< HEAD (ours - current rebase state)\n");
				for (j = conflict_start; j < i; j++) {
					if (j < ours_count && ours_lines[j]) {
						fprintf(file, "%s", ours_lines[j]);
					}
				}
				fprintf(file, "=======\n");
				for (j = conflict_start; j < i; j++) {
					if (j < theirs_count && theirs_lines[j]) {
						fprintf(file, "%s", theirs_lines[j]);
					}
				}
				fprintf(file, ">>>>>>> upstream (incoming change)\n");
				in_conflict = 0;
				conflict_start = -1;
			}
			if (ours_line) {
				fprintf(file, "%s", ours_line);
			}
		} else {
			/* Lines differ - mark start of conflict if not already in one */
			if (!in_conflict) {
				in_conflict = 1;
				conflict_start = i;
			}
			/* Don't write anything yet - accumulate the conflict */
		}
	}

	/* Close any remaining conflict */
	if (in_conflict) {
		fprintf(file, "<<<<<<< HEAD (ours - current rebase state)\n");
		for (j = conflict_start; j < max_lines; j++) {
			if (j < ours_count && ours_lines[j]) {
				fprintf(file, "%s", ours_lines[j]);
			}
		}
		fprintf(file, "=======\n");
		for (j = conflict_start; j < max_lines; j++) {
			if (j < theirs_count && theirs_lines[j]) {
				fprintf(file, "%s", theirs_lines[j]);
			}
		}
		fprintf(file, ">>>>>>> upstream (incoming change)\n");
	}

	free_lines(ours_lines, ours_count);
	free_lines(theirs_lines, theirs_count);
}

static void handle_rebase_conflict(git_repository *repo, git_rebase *rebase)
{
	git_index *index;
	git_index_conflict_iterator *conflicts;
	const git_index_entry *ancestor, *ours, *theirs;
	int has_conflicts = 0;

	printf("  Handling conflicts...\n");

	/* Get the index */
	check_error(git_repository_index(&index, repo), "Failed to get index");

	/* Check for conflicts */
	check_error(git_index_conflict_iterator_new(&conflicts, index), "Failed to create conflict iterator");

	while (git_index_conflict_next(&ancestor, &ours, &theirs, conflicts) == 0) {
		has_conflicts = 1;
		printf("    Conflict in file: %s\n", ours ? ours->path : (theirs ? theirs->path : "unknown"));

		/* Create a file with actual conflict markers from the real content */
		if (ours && theirs) {
			char filepath[1024];
			FILE *file;
			const char *workdir = git_repository_workdir(repo);
			char *ours_content = NULL;
			char *theirs_content = NULL;
			char *ancestor_content = NULL;

			/* Read the actual blob contents */
			ours_content = read_blob_content(repo, &ours->id);
			theirs_content = read_blob_content(repo, &theirs->id);
			if (ancestor) {
				ancestor_content = read_blob_content(repo, &ancestor->id);
			}

			snprintf(filepath, sizeof(filepath), "%s/%s", workdir, ours->path);
			file = fopen(filepath, "w");
			if (file) {
				/* Create line-by-line merge with partial conflict markers */
				write_line_by_line_merge(file, ours_content, theirs_content);
				fclose(file);

				printf("      Created partial conflict markers (only conflicting lines)\n");

				/* Mark as resolved by adding to index */
				git_index_add_bypath(index, ours->path);
			}

			free(ours_content);
			free(theirs_content);
			free(ancestor_content);
		}
	}

	git_index_conflict_iterator_free(conflicts);

	if (has_conflicts) {
		git_signature *sig;
		git_oid commit_id;

		/* Write the index */
		check_error(git_index_write(index), "Failed to write index");

		/* Continue rebase with resolved conflicts */
		check_error(git_signature_new(&sig, "Test User", "test@example.com",
			commit_timestamp++, 0), "Failed to create signature");
		check_error(git_rebase_commit(&commit_id, rebase, NULL, sig, NULL, NULL), "Failed to commit during rebase");
		git_signature_free(sig);
	}

	git_index_free(index);
}

static void demonstrate_rebase_abort(const char *repo_path, const char *upstream_path)
{
	git_repository *repo;
	git_rebase *rebase;
	git_reference *upstream_ref;
	git_annotated_commit *upstream_commit;
	git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;
	git_signature *sig;

	printf("\n=== Demonstrating Rebase Abort ===\n");

	/* Open repository */
	check_error(git_repository_open(&repo, repo_path), "Failed to open repository");

	/* Show history before rebase */
	display_history(repo, "Clone repository history before rebase", 10);

	/* Fetch from upstream */
	fetch_from_upstream(repo, upstream_path);

	/* Get upstream branch reference */
	check_error(git_reference_lookup(&upstream_ref, repo, "refs/remotes/upstream/master"),
		"Failed to lookup upstream/master");
	check_error(git_annotated_commit_from_ref(&upstream_commit, repo, upstream_ref),
		"Failed to get annotated commit");

	/* Create signature with hardcoded timestamp */
	check_error(git_signature_new(&sig, "Test User", "test@example.com",
		commit_timestamp++, 0), "Failed to create signature");

	/* Initialize rebase */
	printf("\nInitiating rebase onto upstream/master...\n");
	check_error(git_rebase_init(&rebase, repo, NULL, upstream_commit, NULL, &rebase_opts),
		"Failed to initialize rebase");

	/* Process first commit */
	{
		git_rebase_operation *operation;
		int error;
		git_oid commit_id;

		error = git_rebase_next(&operation, rebase);
		if (error == 0) {
			printf("Processing first rebase operation...\n");
			printf("  Commit being rebased: %s\n", git_oid_tostr_s(&operation->id));

			/* Actually commit the first operation */
			error = git_rebase_commit(&commit_id, rebase, NULL, sig, NULL, NULL);
			if (error == 0) {
				printf("  First commit successfully rebased as: %s\n", git_oid_tostr_s(&commit_id));
			}

			/* Try to process second commit */
			error = git_rebase_next(&operation, rebase);
			if (error == 0) {
				printf("\nProcessing second rebase operation...\n");
				printf("  Commit being rebased: %s\n", git_oid_tostr_s(&operation->id));
				printf("  (This operation will be aborted)\n");
			}
		}
	}

	/* Show in-progress rebase state */
	printf("\nRebase is in progress. Current HEAD is detached.\n");

	/* Abort the rebase */
	printf("\nAborting rebase mid-operation...\n");
	check_error(git_rebase_abort(rebase), "Failed to abort rebase");
	printf("Rebase aborted successfully.\n");

	/* Show history after abort - should be back to original */
	display_history(repo, "Clone repository history after abort (restored to original)", 10);

	/* Cleanup */
	git_signature_free(sig);
	git_rebase_free(rebase);
	git_annotated_commit_free(upstream_commit);
	git_reference_free(upstream_ref);
	git_repository_free(repo);
}

static void demonstrate_successful_rebase(const char *repo_path, const char *upstream_path)
{
	git_repository *repo;
	git_rebase *rebase;
	git_reference *upstream_ref;
	git_annotated_commit *upstream_commit;
	git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;
	git_rebase_operation *operation;
	git_signature *sig;
	int error;

	printf("\n=== Demonstrating Successful Rebase with Conflict Resolution ===\n");

	/* Open repository */
	check_error(git_repository_open(&repo, repo_path), "Failed to open repository");

	/* Show history before rebase */
	display_history(repo, "Clone repository history before successful rebase", 10);

	/* Create signature with hardcoded timestamp */
	check_error(git_signature_new(&sig, "Test User", "test@example.com",
		commit_timestamp++, 0), "Failed to create signature");

	/* Fetch from upstream */
	fetch_from_upstream(repo, upstream_path);

	/* Get upstream branch reference */
	check_error(git_reference_lookup(&upstream_ref, repo, "refs/remotes/upstream/master"),
		"Failed to lookup upstream/master");
	check_error(git_annotated_commit_from_ref(&upstream_commit, repo, upstream_ref),
		"Failed to get annotated commit");

	/* Initialize rebase */
	printf("Initiating rebase onto upstream/master...\n");
	check_error(git_rebase_init(&rebase, repo, NULL, upstream_commit, NULL, &rebase_opts),
		"Failed to initialize rebase");

	/* Process each rebase operation */
	while ((error = git_rebase_next(&operation, rebase)) == 0) {
		git_oid commit_id;
		git_index *index;

		printf("Processing rebase operation %zu...\n", git_rebase_operation_current(rebase));
		printf("  Commit: %s\n", git_oid_tostr_s(&operation->id));

		/* Check for conflicts */
		check_error(git_repository_index(&index, repo), "Failed to get index");

		if (git_index_has_conflicts(index)) {
			printf("  Conflicts detected!\n");
			handle_rebase_conflict(repo, rebase);
		} else {
			/* No conflicts, proceed with commit */
			error = git_rebase_commit(&commit_id, rebase, NULL, sig, NULL, NULL);
			if (error < 0 && error != GIT_EUNMERGED) {
				check_error(error, "Failed to commit during rebase");
			} else if (error == GIT_EUNMERGED) {
				printf("  Unmerged changes detected, handling...\n");
				handle_rebase_conflict(repo, rebase);
			} else {
				printf("  Successfully rebased commit: %s\n", git_oid_tostr_s(&commit_id));
			}
		}

		git_index_free(index);
	}

	if (error == GIT_ITEROVER) {
		/* Finish the rebase */
		printf("\nFinishing rebase...\n");
		check_error(git_rebase_finish(rebase, sig), "Failed to finish rebase");
		printf("Rebase completed successfully!\n");

		/* Display final history after successful rebase */
		display_history(repo, "Clone repository history after successful rebase", 10);
	} else {
		check_error(error, "Error during rebase");
	}

	/* Cleanup */
	git_signature_free(sig);
	git_rebase_free(rebase);
	git_annotated_commit_free(upstream_commit);
	git_reference_free(upstream_ref);
	git_repository_free(repo);
}

/**
 * This example demonstrates the libgit2 rebase APIs when faced with
 * a conflict.  It also shows how to handle aborting a rebase operation.
 *
 * This does not have:
 *
 * - Robust error handling
 * - Interactive rebase options (pick, reword, squash, fixup)
 * - Complex conflict resolution strategies
 *
 */
int lg2_rebase(git_repository *repo, int argc, char **argv)
{
	UNUSED(repo);
	UNUSED(argc);
	UNUSED(argv);

	printf("=== libgit2 Rebase API Demonstration ===\n\n");

	/* Step 1: Create initial repository with commits */
	create_initial_repository(REPO_PATH);

	/* Step 2: Clone the repository */
	clone_repository(REPO_PATH, CLONE_PATH);

	/* Step 3: Create divergent commits in both repositories */
	create_divergent_commits(REPO_PATH, CLONE_PATH);

	/* Step 4a: Demonstrate aborting a rebase */
	demonstrate_rebase_abort(CLONE_PATH, REPO_PATH);

	/* Step 4b: Demonstrate successful rebase with conflict resolution */
	demonstrate_successful_rebase(CLONE_PATH, REPO_PATH);

	printf("\n=== Demonstration Complete ===\n");
	printf("Repositories created at:\n");
	printf("  Original: %s\n", REPO_PATH);
	printf("  Clone: %s\n", CLONE_PATH);

	return 0;
}
