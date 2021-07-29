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
#ifndef INCLUDE_examples_common_h__
#define INCLUDE_examples_common_h__

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <git2.h>
#include <fcntl.h>

#ifdef _WIN32
# include <io.h>
# include <Windows.h>
# define open _open
# define read _read
# define close _close
# define ssize_t int
# define sleep(a) Sleep(a * 1000)
#else
# include <unistd.h>
#endif

#ifndef PRIuZ
/* Define the printf format specifier to use for size_t output */
#if defined(_MSC_VER) || defined(__MINGW32__)
#	define PRIuZ "Iu"
#else
#	define PRIuZ "zu"
#endif
#endif

#ifdef _MSC_VER
#define snprintf sprintf_s
#define strcasecmp strcmpi
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(*x))
#define UNUSED(x) (void)(x)

#include "args.h"
#include "path.h"

extern int lg2_add(git_repository *repo, int argc, char **argv);
extern int lg2_apply(git_repository *repo, int argc, char **argv);

extern int lg2_branch(git_repository *repo, int argc, char **argv);
extern int lg2_branch_create_from_head(git_repository *repo, char *branch_name);
extern int lg2_branch_delete(git_repository *repo, char *branch_name);

extern int lg2_blame(git_repository *repo, int argc, char **argv);
extern int lg2_cat_file(git_repository *repo, int argc, char **argv);
extern int lg2_checkout(git_repository *repo, int argc, char **argv);
extern int lg2_clone(git_repository *repo, int argc, char **argv);
extern int lg2_commit(git_repository *repo, int argc, char **argv);
extern int lg2_config(git_repository *repo, int argc, char **argv);
extern int lg2_describe(git_repository *repo, int argc, char **argv);
extern int lg2_diff(git_repository *repo, int argc, char **argv);
extern int lg2_fetch(git_repository *repo, int argc, char **argv);
extern int lg2_for_each_ref(git_repository *repo, int argc, char **argv);
extern int lg2_general(git_repository *repo, int argc, char **argv);
extern int lg2_index_pack(git_repository *repo, int argc, char **argv);
extern int lg2_init(git_repository *repo, int argc, char **argv);
extern int lg2_log(git_repository *repo, int argc, char **argv);
extern int lg2_ls_files(git_repository *repo, int argc, char **argv);
extern int lg2_rebase(git_repository *repo, int argc, char **argv);
extern int lg2_ls_remote(git_repository *repo, int argc, char **argv);
extern int lg2_merge(git_repository *repo, int argc, char **argv);
extern int lg2_push(git_repository *repo, int argc, char **argv);
extern int lg2_pull(git_repository *repo, int argc, char **argv);
extern int lg2_remote(git_repository *repo, int argc, char **argv);
extern int lg2_reset(git_repository *repo, int argc, char **argv);
extern int lg2_rev_list(git_repository *repo, int argc, char **argv);
extern int lg2_rev_parse(git_repository *repo, int argc, char **argv);
extern int lg2_show_index(git_repository *repo, int argc, char **argv);
extern int lg2_stash(git_repository *repo, int argc, char **argv);
extern int lg2_status(git_repository *repo, int argc, char **argv);
extern int lg2_submodule(git_repository *repo, int argc, char **argv);
extern int lg2_tag(git_repository *repo, int argc, char **argv);
extern int lg2_interactive_tests(git_repository *repo, int argc, char **argv);

/**
 * Check libgit2 error code, printing error to stderr on failure and
 * exiting the program.
 */
extern void check_lg2(int error, const char *message, const char *extra);

/**
 * Read a file into a buffer
 *
 * @param path The path to the file that shall be read
 * @return NUL-terminated buffer if the file was successfully read, NULL-pointer otherwise
 */
extern char *read_file(const char *path);

/**
 * Ask the user for input via stdin.
 *
 * @param out Pointer to where to store the user's response
 * @param prompt NUL-terminated prompt string
 * @param optional 0 iff optional
 * @return -1 on failure
 */
extern int ask(char **out, const char *prompt, char optional);

/**
 * Exit the program, printing error to stderr
 */
extern void fatal(const char *message, const char *extra);

/**
 * Basic output function for plain text diff output
 * Pass `FILE*` such as `stdout` or `stderr` as payload (or NULL == `stdout`)
 */
extern int diff_output(
	const git_diff_delta*, const git_diff_hunk*, const git_diff_line*, void*);

/**
 * Convert a treeish argument to an actual tree; this will call check_lg2
 * and exit the program if `treeish` cannot be resolved to a tree
 */
extern void treeish_to_tree(
	git_tree **out, git_repository *repo, const char *treeish);

/**
 * A realloc that exits on failure
 */
extern void *xrealloc(void *oldp, size_t newsz);

/**
 * Convert a refish to an annotated commit.
 */
extern int resolve_refish(git_annotated_commit **commit, git_repository *repo, const char *refish);

/**
 * Get a given repo's HEAD.
 */
extern int get_repo_head(git_commit **commit, git_repository *repo);

/**
 * Convert a path relative to the current working directory into a path
 * relative to the repository's working (or base, if bare) directory.
 */
extern void get_repopath_to(char **out_path, const char *target_relpath, git_repository *repo);

/**
 * Opposite of get_repopath_to.
 * Returns the path to (repo-workdir-relative) target relative to
 * the **program**'s current working directory.
 */
extern void get_relpath_to(char **out_path, const char *target_repopath, git_repository *repo);

/**
 * Acquire credentials via command line
 */
extern int cred_acquire_cb(git_credential **out,
		const char *url,
		const char *username_from_url,
		unsigned int allowed_types,
		void *repo_payload);

extern int repoless_cred_acquire_cb(git_credential **out,
		const char *url,
		const char *username_from_url,
		unsigned int allowed_types,
		void *ignored_payload);


/**
 * Request that the user confirm a remote certificate before
 * connecting.
 */
extern int certificate_confirm_cb(struct git_cert *cert,
		int valid,
		const char *host,
		void *payload);

/**
 * Log information related to a signature creation error.
 *
 * @param source_error is the return code from the attempt to get the user's
 * 		signature.
 */
extern void handle_signature_create_error(int source_error);

/**
 * Prints a description of a given git_repository_state and how to return
 * to the default state (GIT_REPOSITORY_STATE_NONE).
 */
extern void print_repo_state_description(git_repository_state_t state);

/**
 * Repeated documentation output.
 */
#define INSTRUCTIONS_FOR_STORING_AUTHOR_INFORMATION \
		  "Try running \n" \
		  "    lg2 config user.name 'Your Name'\n" \
		  "    lg2 config user.email youremail@example.com\n" \
		  "to provide authorship information for new commits " \
		  "in this repository.\n" \
		  "This information is used to label new commits " \
		  "and will travel with them " \
		  "(e.g. it's shared with servers when you `lg2 push`).\n"

// iOS/OSX architecture definitions
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE

#include "ios_error.h"
#define isatty ios_isatty
#define fork   ios_fork
#undef stdin
#undef stdout
#undef stderr
#define stdin thread_stdin
#define stdout thread_stdout
#define stderr thread_stderr
#define printf(args...) fprintf(thread_stdout, args)
#define DOCUMENTATION_EXPECTED_HOMEDIR "~/Documents/"

#else

#define DOCUMENTATION_EXPECTED_HOMEDIR "~/"

#endif


#endif
