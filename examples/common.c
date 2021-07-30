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
#include <limits.h>

#include <git2/sys/features.h>
#ifdef GIT_SSH
#include <libssh2.h>
#endif

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

#ifndef GIT_SSH
	fprintf(stderr,
		"WARNING: libgit2 was not compiled with ssh support. "
		"Authentication will probably fail.\n");
#endif

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
	git_config* cfg = NULL;
	git_config_entry *entry = NULL;

	UNUSED(url);
	/* Get username, password, identityFile from config */
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

/**
 * Print the base-64 encoded version of data to stdout.
 * size is in bytes.
 *
 * See https://en.wikipedia.org/wiki/Base64
 */
static void print_b64(const unsigned char *data, size_t size)
{
	const unsigned char *end = data + size;
	const unsigned char *ptr = data;
	size_t bit_idx = 0;

	if (size >= UINT_MAX / 8 - 1) {
		printf("< Too many bytes to encode >");
		return;
	}

	while (ptr < end) {
		unsigned char current = 0;
		unsigned char next = 0;

		size_t end_idx = bit_idx + 6;
		size_t shift_current = end_idx < 8 ? 8 - end_idx : 0;
		size_t bits_in_next = end_idx < 8 ? 0 : end_idx - 8;

		// The six bits we want are part of the next byte!
		// Fetch it, if possible.
		if (bit_idx > 0 && ptr < end - 1) {
			next = *(ptr + 1);
		}

		current = *ptr >> shift_current;

		current <<= bits_in_next;
		current += (next >> (8 - bits_in_next));

		current &= 0x3F;

		// Here: current is zero except for its last six bits.

		if (current < 26) {
			putchar('A' + (char) current);
		} else if (current >= 26 && current < 26 * 2) {
			current -= 26;
			putchar('a' + (char) current);
		} else if (current >= 26 * 2 && current < 26 * 2 + 10) {
			current -= 26 * 2;
			putchar('0' + (char) current);
		} else if (current == 62) {
			putchar('+');
		} else if (current == 63) {
			putchar('/');
		} else {
			printf("?\n[!!BUG!!] print_b64 logic error.\n");
			exit(1);
		}

		// We handle 6 bits at a time (because 2^6 = 64).
		bit_idx += 6;

		// If bit_idx > 8, we've advanced a byte.
		// Preserves invariant: bit_idx \in [0, 8)
		if (bit_idx >= 8) {
			bit_idx = bit_idx % 8;
			ptr++;
		}
	}

	// Padding.
	for (; bit_idx % 8 != 0; bit_idx += 6) {
		putchar('=');
	}
}

#ifdef GIT_SSH
static void print_hostkey_hash(const git_cert_hostkey *ssh_cert)
{
	if (ssh_cert->type & GIT_CERT_SSH_MD5) {
		printf("MD5: ");
		print_b64(ssh_cert->hash_md5, sizeof(ssh_cert->hash_md5));
		printf("\n");
	}

	if (ssh_cert->type & GIT_CERT_SSH_SHA1) {
		printf("SHA-1: ");
		print_b64(ssh_cert->hash_sha1, sizeof(ssh_cert->hash_sha1));
		printf("\n");
	}

	if (ssh_cert->type & GIT_CERT_SSH_SHA256) {
		printf("SHA-256: ");
		print_b64(ssh_cert->hash_sha256, sizeof(ssh_cert->hash_sha256));
		printf("\n");
	}
}

static char * get_knownhosts_filepath(void)
{
	char *result = NULL;
	const char *home = getenv("SSH_HOME");

	if (home == NULL)
		home = getenv("HOME");

	if (home == NULL)
		return NULL;

	join_paths(&result, home, ".ssh/known_hosts");
	return result;
}

/**
 * Returns the LIBSSH2_KNOWNHOST_KEYENC type corresponding to the given ssh cert.
 * Returns zero or LIBSSH2_KNOWNHOST_KEY_UNKNOWN on failure.
 */
static int get_libssh2_cert_type(git_cert_hostkey *cert)
{
	if ((cert->type & GIT_CERT_SSH_RAW) == 0) {
		fprintf(stderr, "WARNING: No raw information associated with the certificate.\n");
		return 0;
	}

	switch (cert->raw_type) {
		case GIT_CERT_SSH_RAW_TYPE_RSA:
			return LIBSSH2_KNOWNHOST_KEY_SSHRSA;
		case GIT_CERT_SSH_RAW_TYPE_DSS:
			return LIBSSH2_KNOWNHOST_KEY_SSHDSS;
		case GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_256:
			return LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
		case GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_384:
			return LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
		case GIT_CERT_SSH_RAW_TYPE_KEY_ECDSA_521:
			return LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
		case GIT_CERT_SSH_RAW_TYPE_KEY_ED25519:
			return LIBSSH2_KNOWNHOST_KEY_ED25519;
		default:
			break;
	}

	fprintf(stderr, "WARNING: Unknown remote certificate raw_type!\n");
	return LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
}

static int ask_add_knownhost_key(LIBSSH2_SESSION *session, LIBSSH2_KNOWNHOSTS *hosts,
		const char *hostname, const char *key, size_t key_len, int libssh2_cert_rawtype)
{
	char *buf = NULL;
	int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | libssh2_cert_rawtype;
	int result = 0;

	// We need mutable copies of the hostname and key
	char *hostname_copy = malloc(strlen(hostname) + 1);
	char *key_copy = malloc(key_len + 1);
	memcpy(key_copy, key, key_len + 1);
	strcpy(hostname_copy, hostname);

	printf(
		"Would you like to add the following host/key pair to your known_hosts"
		" file?\n");
	printf("Hostname: %s\n", hostname);
	printf("Key: ");
	print_b64((unsigned char *) key, key_len);
	printf("\n");

	ask(&buf, "Add the host/key pair? y/[n]", 1);
	if (buf == NULL || buf[0] != 'y' || buf[1] != '\0') {
		printf("Not adding hostname/key pair (expected y or n).\n");
		result = -1;
		goto cleanup;
	}

	result = libssh2_knownhost_addc(hosts, hostname_copy, NULL,
				key_copy, key_len, NULL, 0, typemask, NULL);
	if (result != 0) {
		free(buf);
		libssh2_session_last_error(session, &buf, NULL, 1);

		fprintf(stderr, "Error adding key: %s\n", buf);
		goto cleanup;
	}

cleanup:
	free(hostname_copy);
	free(key_copy);
	free(buf);

	return result;
}

static int is_host_unknown(git_cert_hostkey *cert, const char *hostname)
{
	int error = -1; // Assume failure unless set otherwise.
	int num_knownhosts = 0;
	int updated_knownhosts = 0;

	LIBSSH2_SESSION *session = NULL;
	LIBSSH2_KNOWNHOSTS *hosts = NULL;
	const char *key = NULL;
	int typemask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
	size_t key_len = 0;

	char *knownhosts_filepath = get_knownhosts_filepath();
	if (knownhosts_filepath == NULL) {
		fprintf(stderr, "Unable to determine location of SSH_CONFIG_DIR/known_hosts file\n");
		return -1;
	}

	session = libssh2_session_init();

	if (session == NULL) {
		fprintf(stderr, "Unable to open a libssh2 session!\n");

		goto cleanup;
	}

	hosts = libssh2_knownhost_init(session);
	num_knownhosts = libssh2_knownhost_readfile(hosts, knownhosts_filepath, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
	if (num_knownhosts < 0) {
		char *errmsg = NULL;
		libssh2_session_last_error(session, &errmsg, NULL, 1);

		fprintf(stderr, "Unable to read known_hosts file: %s. Error: %s.\n",
				knownhosts_filepath, errmsg);
		num_knownhosts = 0;

		free(errmsg);

		// Continue with an empty known_hosts file.
	}

	printf("There are %d known hosts...\n", num_knownhosts);

	if ((cert->type & GIT_CERT_SSH_RAW) == 0) {
		fprintf(stderr, "Raw certificate data is unavailable. Unable to check host.\n");
		error = -1;
		goto cleanup;
	}

	key = cert->hostkey;
	key_len = cert->hostkey_len;

	error = libssh2_knownhost_check(hosts, hostname, key, key_len, typemask, NULL);
	switch (error) {
		case LIBSSH2_KNOWNHOST_CHECK_MATCH:
			printf("Host %s is in known_hosts!\n", hostname);
			error = 0;
			break;
		case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
			fprintf(stderr,
					"Error encountered while checking the"
					"known hosts file (%s) for %s!\n",
					knownhosts_filepath, hostname);
			error = 1;
			break;
		case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
			fprintf(stderr, "No key was found for %s in %s.\n",
					hostname, knownhosts_filepath);

			error = ask_add_knownhost_key(session, hosts, hostname, key, key_len,
					get_libssh2_cert_type(cert));
			updated_knownhosts = !error;
			break;
		case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
			fprintf(stderr,
					"Warning: Key for %s does not match that in known_hosts! \n"
					"    \n"
					"    Please ensure that you really are connecting to the correct \n"
					"    host.\n\n",
					hostname);
			error = ask_add_knownhost_key(session, hosts, hostname, key, key_len,
					get_libssh2_cert_type(cert));
			updated_knownhosts = !error;
			break;
	}


	if (updated_knownhosts) {
		error = libssh2_knownhost_writefile(hosts, knownhosts_filepath,
				LIBSSH2_KNOWNHOST_FILE_OPENSSH);

		if (error) {
			fprintf(stderr, "Error while writing to %s.\n", knownhosts_filepath);
		}
	}


cleanup:
	libssh2_knownhost_free(hosts);
	libssh2_session_free(session);
	free(knownhosts_filepath);

	return error;
}

#endif

int certificate_confirm_cb(struct git_cert *cert,
		int valid,
		const char *hostname,
		void *payload)
{
	char* do_connect = NULL;

	UNUSED(payload);
	if (valid) {
		// If certificate is valid, proceed with the connection.
		printf("Connecting to %s...\n", hostname);
		return 0;
	}

#ifdef GIT_SSH
	/**
	 * At the time of this writing, libgit2 states that "we don't currently
	 * trust any hostkeys".
	 *
	 * As such, we need to check known_hosts ourselves.
	 */
	if (cert->cert_type == GIT_CERT_HOSTKEY_LIBSSH2) {
		git_cert_hostkey *ssh_cert = (git_cert_hostkey *) cert;

		printf("\nHost: %s\n", hostname);
		printf("Public key hashes:\n");
		print_hostkey_hash(ssh_cert);
		printf("\n");

		if (!is_host_unknown(ssh_cert, hostname)) {
			// If it's known, its certificate is valid.
			return 0;
		}
	}
#else
	// No SSH? Don't connect to an SSH server.
	if (cert->cert_type == GIT_CERT_HOSTKEY_LIBSSH2) {
		fprintf(stderr,
			"WARNING: libgit2 was not compiled with SSH support,"
			" which is **required** to connect to this host.\n");
		return -1;
	}
#endif

	printf("Certificate for host '%s' may not be valid.\n", hostname);
	ask(&do_connect, "Connect anyway? yes/[n] ", 0);

	if (do_connect != NULL && strcmp(do_connect, "yes") == 0) {
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

void print_repo_state_description(git_repository_state_t state) {
	fprintf(stderr, "repository is in state %d\n", state);

	if (state == GIT_REPOSITORY_STATE_MERGE) {
		fprintf(stderr, "It looks like a merge is in progress. Either "
			"resolve the conflicts (see `lg2 status`), `lg2 add` each changed "
			"file and commit the result, or run `lg2 reset --hard HEAD` to stop the "
			"merge.\n");
	} else if (state == GIT_REPOSITORY_STATE_REBASE
			|| state == GIT_REPOSITORY_STATE_REBASE_INTERACTIVE
			|| state == GIT_REPOSITORY_STATE_REBASE_MERGE) {
		fprintf(stderr, "It looks like a rebase is in progress. "
				"If you want to cancel the rebase, run `lg2 rebase --abort`.\n");
	} else if (state == GIT_REPOSITORY_STATE_NONE) {
		fprintf(stderr, "This is the default state. Run lg2 rebase remote/branch"
				" to rebase onto a branch, lg2 merge remote/branch to merge a branch"
				" into the current.\n");
	}
}

static int interactive_tests(void)
{
	char *buf = NULL;
	int error = 0;

	printf("[...] Running lg2's test suite. Some tests require user interaction.\n");

	if (test_path_lib()) fatal("Pathlib tests failed.", NULL);

	error = ask(&buf, "Prompt test [Type the lowercase letter 'y' to pass]:", 0);
	if (error || buf == NULL) fatal("Unable to read user input!", NULL);

	if (buf[0] != 'y' || buf[1] != '\0') fatal("Input did not match the expected.", NULL);
	free(buf);

	printf("The following two lines should match:\n%s\n", "VGhpcyBtdXN0IHBhc3Mu");
	print_b64((unsigned char*) "This must pass.", sizeof(char) * strlen("This must pass."));
	printf("\n\n");

	printf("The following two lines should match:\n%s\n", "IT1Bbm90aGVyIHRlc3Q9IQ==");
	print_b64((unsigned char*) "!=Another test=!", sizeof(char) * strlen("!=Another test=!"));
	printf("\n\n");

	print_b64((unsigned char*) "A", 1);
	printf("\n");

	return 0;
}

int lg2_interactive_tests(git_repository *repo, int argc, char **argv)
{
	UNUSED(repo);
	UNUSED(argc);
	UNUSED(argv);

	return interactive_tests();
}

