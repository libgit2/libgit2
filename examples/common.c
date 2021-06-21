/*
 * Utilities library for libgit2 examples
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

#ifndef _WIN32
# include <unistd.h>
# include <sys/types.h>
# include <dirent.h>
#endif
#include <errno.h>

void check_lg2(int error, const char *message, const char *extra)
{
	const git_error *lg2err;
	const char *lg2msg = "", *lg2spacer = "";

	if (!error)
		return;

	if ((lg2err = git_error_last()) != NULL && lg2err->message != NULL) {
		lg2msg = lg2err->message;
		lg2spacer = " - ";
	}

	if (extra)
		fprintf(stderr, "%s '%s' [%d]%s%s\n",
			message, extra, error, lg2spacer, lg2msg);
	else
		fprintf(stderr, "%s [%d]%s%s\n",
			message, error, lg2spacer, lg2msg);

	exit(1);
}

void fatal(const char *message, const char *extra)
{
	if (extra)
		fprintf(stderr, "%s %s\n", message, extra);
	else
		fprintf(stderr, "%s\n", message);

	exit(1);
}

int diff_output(
	const git_diff_delta *d,
	const git_diff_hunk *h,
	const git_diff_line *l,
	void *p)
{
	FILE *fp = (FILE*)p;

	(void)d; (void)h;

	if (!fp)
		fp = stdout;

	if (l->origin == GIT_DIFF_LINE_CONTEXT ||
		l->origin == GIT_DIFF_LINE_ADDITION ||
		l->origin == GIT_DIFF_LINE_DELETION)
		fputc(l->origin, fp);

	fwrite(l->content, 1, l->content_len, fp);

	return 0;
}

void treeish_to_tree(
	git_tree **out, git_repository *repo, const char *treeish)
{
	git_object *obj = NULL;

	check_lg2(
		git_revparse_single(&obj, repo, treeish),
		"looking up object", treeish);

	check_lg2(
		git_object_peel((git_object **)out, obj, GIT_OBJECT_TREE),
		"resolving object to tree", treeish);

	git_object_free(obj);
}

void *xrealloc(void *oldp, size_t newsz)
{
	void *p = realloc(oldp, newsz);
	if (p == NULL) {
		fprintf(stderr, "Cannot allocate memory, exiting.\n");
		exit(1);
	}
	return p;
}

int resolve_refish(git_annotated_commit **commit, git_repository *repo, const char *refish)
{
	git_reference *ref;
	git_object *obj;
	int err = 0;

	assert(commit != NULL);

	err = git_reference_dwim(&ref, repo, refish);
	if (err == GIT_OK) {
		git_annotated_commit_from_ref(commit, repo, ref);
		git_reference_free(ref);
		return 0;
	}

	err = git_revparse_single(&obj, repo, refish);
	if (err == GIT_OK) {
		err = git_annotated_commit_lookup(commit, repo, git_object_id(obj));
		git_object_free(obj);
	}

	return err;
}

int get_repo_head(git_commit **head, git_repository *repo)
{
	git_oid head_id;
	int error = 0;

	error = git_reference_name_to_id(&head_id, repo, "HEAD");
	if (error != 0) {
		fprintf(stderr, "failed to resolve HEAD.\n");
		goto cleanup;
	}

	error = git_commit_lookup(head, repo, &head_id);
	if (error != 0 || *head == NULL) {
		printf("Error looking up HEAD's commit.\n");
		goto cleanup;
	}

cleanup:
	return error;
}

static const char * repo_base_path(git_repository *repo)
{
	const char *workdir_path = git_repository_workdir(repo);

	// If we don't have a working directory for the repository,
	// default to the directory we would put the ".git" folder in.
	if (workdir_path == NULL) {
		workdir_path = git_repository_path(repo);
	}

	return workdir_path;
}

void get_repopath_to(char **out_path, const char *target, git_repository *repo)
{
	const char *workdir_path = repo_base_path(repo);

	path_relative_to(out_path, target, workdir_path);
}

void get_relpath_to(char **out_path, const char *target_path, git_repository *repo)
{
	const char *repo_path = repo_base_path(repo);
	char *program_path = getcwd(NULL, 0);

	const char *target_abspath = NULL;
	char *target_abspath_builder = NULL;

	if (target_path[0] != '/') {
		join_paths(&target_abspath_builder, repo_path, target_path);
		target_abspath = target_abspath_builder;
	} else {
		target_abspath = target_path;
	}

	path_relative_to(out_path, target_abspath, program_path);

	free(target_abspath_builder);
	free(program_path);
}

static int readline(char **out)
{
	int c, error = 0, length = 0, allocated = 0;
	char *line = NULL;

	errno = 0;

	while ((c = getchar()) != EOF) {
		if (length == allocated) {
			allocated += 16;

			if ((line = realloc(line, allocated)) == NULL) {
				error = -1;
				goto error;
			}
		}

		if (c == '\n')
			break;

		line[length++] = c;
	}

	if (errno != 0) {
		error = -1;
		goto error;
	}

	// We encountered an EOF (line == NULL -> first
	// getchar() was null).
	if (line == NULL) {
		error = -1;
		errno = EIO; // Report generic IO error.
		goto error;
	}

	line[length] = '\0';
	*out = line;
	line = NULL;
	error = length;
error:
	free(line);
	return error;
}

int ask(char **out, const char *prompt, char optional)
{
	printf("%s ", prompt);
	fflush(stdout);

	if (readline(out) <= 0 && !optional) {
		fprintf(stderr, "Could not read response: %s\n",
				errno != 0 ? strerror(errno) : "No message");
		return -1;
	}

	return 0;
}

static int ask_for_ssh_key(char **privkey, const char *suggested_keys_directory)
{
	int result = 0;
	int num_suggestions = 0;
	int answer_asnum = -1;
	int i;
	char **suggestions = NULL;
	int listing_failure = 0;

#ifndef _WIN32
	int dir_fd = open(suggested_keys_directory, O_DIRECTORY | O_RDONLY);
	DIR *dir = NULL;

	if (dir_fd != -1) {
		dir = fdopendir(dir_fd);
	}

	if (dir != NULL) {
		struct dirent *entry = NULL;

		printf("SSH keys in %s:\n", suggested_keys_directory);

		// On POSIX, we can list the contents of a directory:
		while ((entry = readdir(dir)) != NULL) {
			const char *name = entry->d_name;
			const char *ext = file_extension_from_path(name);

			// Only count id_*, but not public key files.
			if (strncmp(name, "id_", 3) == 0 && strcmp(ext, ".pub") != 0) {
				num_suggestions ++;
				printf(" %d\t\t%s\n", num_suggestions, name);

				// Check for overflow.
				if (num_suggestions < 0) {
					fprintf(stderr, "Too many paths to list.\n");
					return -1;
				}
			}
		}

		rewinddir(dir);
		suggestions = (char **) malloc(sizeof(char *) * num_suggestions);
		i = 0;

		while ((entry = readdir(dir)) != NULL) {
			const char *name = entry->d_name;
			const char *ext = file_extension_from_path(name);

			if (strncmp(name, "id_", 3) == 0 && strcmp(ext, ".pub") != 0
						&& i < num_suggestions) {
				join_paths(&suggestions[i], suggested_keys_directory, name);
				i++;
			}
		}

		for (; i < num_suggestions; i++) {
			suggestions[i] = NULL;
		}

		if (num_suggestions == 0) {
			printf(" [ No suggested keys ] \n");
		}

		printf("\n");
		printf("Enter the number to the left of the desired key ");
		printf("or the path to some other SSH key (the private key).\n");

		closedir(dir);
	} else {
		fprintf(stderr, "Warning: Unable to list keys in %s: %s (%d).\n",
				suggested_keys_directory, strerror(errno), errno);
		listing_failure = 1;
	}

#else
	listing_failure = 1;
#endif

	if (listing_failure) {
		printf("Enter the path to a private SSH key.\n");
	}

	result = ask(privkey, "SSH Key:", 0);

	if (result >= 0) {
		answer_asnum = atoi(*privkey);

		// Try to convert a number into one of the listed ssh keys.
		if (answer_asnum > 0 && answer_asnum < num_suggestions + 1) {
			const char *suggestion = suggestions[answer_asnum - 1];

			free(*privkey);
			*privkey = strcpy((char *) malloc(strlen(suggestion) + 1), suggestion);
		}
	}

//cleanup:
	for (i = 0; i < num_suggestions; i++) {
		free(suggestions[i]);
	}
	free(suggestions);

	return result;
}

int cred_acquire_cb(git_credential **out,
		const char *url,
		const char *username_from_url,
		unsigned int allowed_types,
		void *payload)
{
	char *username = NULL, *password = NULL, *privkey = NULL, *pubkey = NULL;
	git_repository* repo = (git_repository*) payload;
	int error = 1;
	// iOS addition: let's get the config file
	git_config* cfg = NULL;
	git_config_entry *entry = NULL;

	UNUSED(url);
	/* UNUSED(payload); */
	/* iOS addition: get username, password, identityFile from config */
	if (repo != NULL)
		error = git_repository_config(&cfg, repo);
	else
		error = git_config_open_default(&cfg);

	if (username_from_url) {
		if ((username = strdup(username_from_url)) == NULL)
			goto out;
	} else {
		if (cfg != NULL) {
			error = git_config_get_entry(&entry, cfg, "user.name");
			if (error >= 0) {
				username = strdup(entry->value);
			}
		}
		if (username == NULL) {
			if ((error = ask(&username, "Username:", 0)) < 0) {
				goto out;
			}
		}
	}

	if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
		int n;
		char* home = getenv("SSH_HOME");
		if (home == NULL)
			home = getenv("HOME");

		if (cfg != NULL) {
			error = git_config_get_entry(&entry, cfg, "user.identityFile");
			if (error >= 0) {
				if (home != NULL
						// Use the value if it's an absolute path.
						&& strncmp(entry->value, "~", 1) != 0
						&& strncmp(entry->value, "/", 1) != 0) {
					n = snprintf(NULL, 0, "%s/.ssh/%s", home, entry->value);
					privkey = malloc(n + 1);
					if (privkey != NULL) {
						snprintf(privkey, n + 1, "%s/.ssh/%s", home, entry->value);
					}
				} else {
					privkey = strdup(entry->value);
				}

				printf("SSH authentication: Using private key: %s\n", privkey);
				error = git_config_get_entry(&entry, cfg, "user.password");
				if (error >= 0)
					password = strdup(entry->value);
				else
					error = ask(&password, "Password:", 1);
			} else {
				const git_error* err = git_error_last();
				printf("No user.identityFile found in git config: %s.\n", err->message);
			}
		}

		if (privkey == NULL) {
			char *suggested_ssh_path = NULL;
			n = snprintf(NULL, 0, "%s/.ssh/", home);

			suggested_ssh_path = (char *) malloc(n + 1);
			snprintf(suggested_ssh_path, n + 1, "%s/.ssh/", home);

			if ((error = ask_for_ssh_key(&privkey, suggested_ssh_path)) < 0 ||
					(error = ask(&password, "Password:", 1)) < 0) {
				free(suggested_ssh_path);
				goto out;
			}
			printf("Consider running,\n");
			printf("    lg2 config user.identityFile '%s'\n", privkey);
			if (strcmp(password, "") != 0) {
				printf("    lg2 config user.password 'your_password_here'\n");
			} else {
				printf("    lg2 config user.password \"\"\n");
			}
			printf("to save this username/password pair.\n\n");

			free(suggested_ssh_path);
		}

		// For compatability with iOS, we only expand ~/
		expand_path(&privkey);

		if ((n = snprintf(NULL, 0, "%s.pub", privkey)) < 0 ||
		    (pubkey = malloc(n + 1)) == NULL ||
		    (n = snprintf(pubkey, n + 1, "%s.pub", privkey)) < 0)
			goto out;

		error = git_credential_ssh_key_new(out, username, pubkey, privkey, password);
	} else if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
		if (cfg != NULL) {
			error = git_config_get_entry(&entry, cfg, "user.password");
			if (error >= 0) {
				password = strdup(entry->value);
			}
		}
		if (password == NULL) {
			if ((error = ask(&password, "Password:", 1)) < 0) {
				goto out;
			}
		}

		error = git_credential_userpass_plaintext_new(out, username, password);
	} else if (allowed_types & GIT_CREDENTIAL_USERNAME) {
		error = git_credential_username_new(out, username);
	}

out:
	free(username);
	free(password);
	free(privkey);
	free(pubkey);
	git_config_free(cfg);
	return error;
}

int repoless_cred_acquire_cb(git_credential **out,
		const char *url,
		const char *username_from_url,
		unsigned int allowed_types,
		void *ignored_payload)
{
	UNUSED(ignored_payload);
	return cred_acquire_cb(out, url, username_from_url, allowed_types, NULL);
}

int certificate_confirm_cb(struct git_cert *cert,
		int valid,
		const char *host,
		void *payload)
{
	char* do_connect = NULL;

	UNUSED(cert);
	UNUSED(payload);
	if (valid) {
		// If certificate is valid, proceed with the connection.
		printf("Connecting to %s...\n", host);
		return 0;
	}

	printf("Certificate for host '%s' may not be valid.\n", host);
	ask(&do_connect, "Connect anyway? y/[n] ", 0);

	if (do_connect != NULL && strcmp(do_connect, "y") == 0) {
		printf("Connecting anyway...\n");
		return 0;
	} else {
		return -1; // Don't connect.
	}
}

void handle_signature_create_error(int source_err)
{
	const git_error *err = git_error_last();
	fprintf(stderr, "Error creating signature.\n");

	if ((err && err->klass == GIT_ERROR_CONFIG) || source_err == GIT_ENOTFOUND) {
		fprintf(stderr, "This seems to be a configuration error, ");
		fprintf(stderr,
			"probably the result of missing or invalid "
			"author information.\n");
		fprintf(stderr, INSTRUCTIONS_FOR_STORING_AUTHOR_INFORMATION);
	}
}

char *read_file(const char *path)
{
	ssize_t total = 0;
	char *buf = NULL;
	struct stat st;
	int fd = -1;

	if ((fd = open(path, O_RDONLY)) < 0 || fstat(fd, &st) < 0)
		goto out;

	if ((buf = malloc(st.st_size + 1)) == NULL)
		goto out;

	while (total < st.st_size) {
		ssize_t bytes = read(fd, buf + total, st.st_size - total);
		if (bytes <= 0) {
			if (errno == EAGAIN || errno == EINTR)
				 continue;
			free(buf);
			buf = NULL;
			goto out;
		}
		total += bytes;
	}

	buf[total] = '\0';

out:
	if (fd >= 0)
		close(fd);
	return buf;
}

