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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <git2.h>

#ifndef PRIuZ
/* Define the printf format specifer to use for size_t output */
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

extern int lg2_add(git_repository *repo, int argc, char **argv);
extern int lg2_blame(git_repository *repo, int argc, char **argv);
extern int lg2_cat_file(git_repository *repo, int argc, char **argv);
extern int lg2_checkout(git_repository *repo, int argc, char **argv);
extern int lg2_clone(git_repository *repo, int argc, char **argv);
extern int lg2_describe(git_repository *repo, int argc, char **argv);
extern int lg2_diff(git_repository *repo, int argc, char **argv);
extern int lg2_fetch(git_repository *repo, int argc, char **argv);
extern int lg2_for_each_ref(git_repository *repo, int argc, char **argv);
extern int lg2_general(git_repository *repo, int argc, char **argv);
extern int lg2_index_pack(git_repository *repo, int argc, char **argv);
extern int lg2_init(git_repository *repo, int argc, char **argv);
extern int lg2_log(git_repository *repo, int argc, char **argv);
extern int lg2_ls_files(git_repository *repo, int argc, char **argv);
extern int lg2_ls_remote(git_repository *repo, int argc, char **argv);
extern int lg2_merge(git_repository *repo, int argc, char **argv);
extern int lg2_remote(git_repository *repo, int argc, char **argv);
extern int lg2_rev_list(git_repository *repo, int argc, char **argv);
extern int lg2_rev_parse(git_repository *repo, int argc, char **argv);
extern int lg2_show_index(git_repository *repo, int argc, char **argv);
extern int lg2_status(git_repository *repo, int argc, char **argv);
extern int lg2_tag(git_repository *repo, int argc, char **argv);

/**
 * Check libgit2 error code, printing error to stderr on failure and
 * exiting the program.
 */
extern void check_lg2(int error, const char *message, const char *extra);

/**
 * Exit the program, printing error to stderr
 */
extern void fatal(const char *message, const char *extra);

/**
 * Check if a string has the given prefix.  Returns 0 if not prefixed
 * or the length of the prefix if it is.
 */
extern size_t is_prefixed(const char *str, const char *pfx);

/**
 * Match an integer string, returning 1 if matched, 0 if not.
 */
extern int is_integer(int *out, const char *str, int allow_negative);

struct args_info {
	int    argc;
	char **argv;
	int    pos;
};
#define ARGS_INFO_INIT { argc, argv, 0 }

/**
 * Check current `args` entry against `opt` string.  If it matches
 * exactly, take the next arg as a string; if it matches as a prefix with
 * an equal sign, take the remainder as a string; if value not supplied, 
 * default value `def` will be given. otherwise return 0.
 */
extern int optional_str_arg(
	const char **out, struct args_info *args, const char *opt, const char *def);

/**
 * Check current `args` entry against `opt` string.  If it matches
 * exactly, take the next arg as a string; if it matches as a prefix with
 * an equal sign, take the remainder as a string; otherwise return 0.
 */
extern int match_str_arg(
	const char **out, struct args_info *args, const char *opt);

/**
 * Check current `args` entry against `opt` string parsing as uint16.  If
 * `opt` matches exactly, take the next arg as a uint16_t value; if `opt`
 * is a prefix (equal sign optional), take the remainder of the arg as a
 * uint16_t value; otherwise return 0.
 */
extern int match_uint16_arg(
	uint16_t *out, struct args_info *args, const char *opt);

/**
 * Check current `args` entry against `opt` string parsing as uint32.  If
 * `opt` matches exactly, take the next arg as a uint16_t value; if `opt`
 * is a prefix (equal sign optional), take the remainder of the arg as a
 * uint32_t value; otherwise return 0.
 */
extern int match_uint32_arg(
	uint32_t *out, struct args_info *args, const char *opt);

/**
 * Check current `args` entry against `opt` string parsing as int.  If
 * `opt` matches exactly, take the next arg as an int value; if it matches
 * as a prefix (equal sign optional), take the remainder of the arg as a
 * int value; otherwise return 0.
 */
extern int match_int_arg(
	int *out, struct args_info *args, const char *opt, int allow_negative);

/**
 * Check current `args` entry against a "bool" `opt` (ie. --[no-]progress).
 * If `opt` matches positively, out will be set to 1, or if `opt` matches
 * negatively, out will be set to 0, and in both cases 1 will be returned.
 * If neither the positive or the negative form of opt matched, out will be -1,
 * and 0 will be returned.
 */
extern int match_bool_arg(int *out, struct args_info *args, const char *opt);

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
 * Acquire credentials via command line
 */
extern int cred_acquire_cb(git_cred **out,
		const char *url,
		const char *username_from_url,
		unsigned int allowed_types,
		void *payload);
