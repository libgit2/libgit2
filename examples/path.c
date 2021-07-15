/**
 * Path-processing utilities for the libgit2 examples.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "path.h"

#define PATH_DELIM_S "/"
#define PATH_DELIM '/'
#define DIRUP_STR_LEN 3

static char * expand_to_new_path(const char *path);

void path_relative_to(char **out_path, const char *target_path, const char *relto)
{
	char *relto_abs = expand_to_new_path(relto);
	char *target_path_abs = expand_to_new_path(target_path);
	char *result = NULL;

	size_t keep_start = 0, dirs_up = 0, outpath_len = 0, last_div_location = 0;
	size_t relto_abs_len = strlen(relto_abs);
	size_t target_abs_len = strlen(target_path_abs);
	size_t i;

	*out_path = NULL;

	for (i = 0; i <= relto_abs_len && i <= target_abs_len; i++) {
		if (relto_abs[i] != target_path_abs[i]) {
			break;
		} else if (relto_abs[i] == PATH_DELIM) {
			last_div_location = i;
		}
	}

	// Handles the following case:
	// 	/foo/bar
	// 	/foo/bar/baz
	// last_div_location should point to the end of
	// '/foo/bar', even though both don't end in PATH_DELIM.
	if (i == relto_abs_len && target_path_abs[i] == PATH_DELIM) {
		last_div_location = i;
	} else if (i == target_abs_len && relto_abs[i] == PATH_DELIM) {
		last_div_location = i;
	}

	// Handle the following case:
	// 	/Makefile
	// 	/Makefile_includes/foo
	// Here, keep_start should point to
	// the beginning of Makefile, not the
	// end of Makefile_includes
	keep_start = last_div_location;

	// Here, i is the index at which the paths differ (or the end of the shorter).

	if (keep_start == relto_abs_len) {
		// Case: target_path_abs points to a subdirectory of relto_path_abs

		dirs_up = 0;
	} else {
		// Case: target_path_abs points to a directory that is in a parent
		//       directory of relto_path_abs
		dirs_up = 0;

		for (; i < relto_abs_len; i++) {
			if (relto_abs[i] == PATH_DELIM) {
				dirs_up++;
			}
		}

		// We know there is at least one directory we need
		// to go up (because we're in this case). Above,
		// we assume that each directory in the list ends with '/'...
		if (relto_abs[relto_abs_len - 1] != PATH_DELIM) {
			dirs_up++;
		}
	}

	if (target_path_abs[keep_start] == '/') {
		keep_start++;
	}
	outpath_len = dirs_up * DIRUP_STR_LEN + target_abs_len - keep_start;

	if (outpath_len > 0) {
		result = (char *) malloc(outpath_len + 1);

		// Add leading '../'s
		for (i = 0; i < dirs_up; i++) {
			result[i*3] = '.';
			result[i*3 + 1] = '.';
			result[i*3 + 2] = PATH_DELIM;
		}

		// Fill part we're keeping.
		for (i = keep_start; target_path_abs[i] != '\0'; i++) {
			int offset = i - keep_start;
			int start  = dirs_up * 3;

			result[start + offset] = target_path_abs[i];
		}
	} else {
		// The two paths are the same, so, the relative path is
		// just "./"

		outpath_len = 2;
		result = (char *) malloc(outpath_len + 1);

		result[0] = '.';
		result[1] = PATH_DELIM;
	}

	result[outpath_len] = '\0';
	*out_path = result;
	free(relto_abs);
	free(target_path_abs);
}

static char * expand_to_new_path(const char *path)
{
	size_t path_len = strlen(path);
	char *copy = (char *) malloc(path_len + 1);

	strncpy(copy, path, path_len + 1);
	expand_path(&copy);

	return copy;
}

void expand_path(char **path)
{
	const char *home = getenv("HOME");
	char *new_path = NULL;
	char *read_from = NULL, *write_to = NULL;

	if ((strncmp(*path, "~" PATH_DELIM_S, 2) == 0) && (home != NULL)) {
		// Allocate the new path.
		join_paths(&new_path, home, *path + 1);

		if (new_path != NULL) {
			free(*path);
			*path = new_path;
		}
	}

	// If a relative path, convert it to an absolute path.
	if (**path != PATH_DELIM) {
		char *cwd = getcwd(NULL, 0);

		join_paths(&new_path, cwd, *path);

		free(cwd);
		free(*path);

		*path = new_path;
	}

	assert(**path == PATH_DELIM);

	// Process double slashes and ../s.
	for (write_to = *path, read_from = *path;
				*read_from != '\0';
				read_from++, write_to++) {
		if (strncmp(read_from, "/.." PATH_DELIM_S, 4) == 0
					&& write_to > *path) {
			// ../ case

			while (*write_to == PATH_DELIM && write_to > *path) {
				write_to--;
			}

			while (*write_to != PATH_DELIM && write_to > *path) {
				write_to--;
			}

			read_from += 2; // strlen(../) - 1; Handled ..
			write_to --;
			continue;
		} else if (write_to - 1 > *path
				&& *read_from == PATH_DELIM
				&& *(write_to - 1) == PATH_DELIM) {
			// "//" case
			write_to --;
			continue;
		}

		*write_to = *read_from;
	}

	// Ensure we've null-terminated the string.
	*write_to = '\0';

	// Path should start with '/'.
	assert(**path == PATH_DELIM);
}

void join_paths(char **out, const char *left, const char *right)
{
	size_t left_len = strlen(left);
	size_t result_len = 0;
	char *format_str = "%s" PATH_DELIM_S "%s";

	if (left[left_len - 1] == PATH_DELIM) {
		format_str = "%s%s";
	}

	result_len = snprintf(NULL, 0, format_str, left, right);

	*out = (char *) malloc(result_len + 1);
	if (*out != NULL) {
		snprintf(*out, result_len + 1, format_str, left, right);
	}
}

const char * file_extension_from_path(const char * path)
{
	size_t len = strlen(path);
	const char *end = path + len;
	const char *ptr = NULL;
	const char *ext_ptr = end;

	for (ptr = path; ptr < end; ptr++) {
		if (*ptr == '/') {
			ext_ptr = end;
		} else if (*ptr == '.') {
			ext_ptr = ptr;
		}
	}

	return ext_ptr;
}

int test_path_lib()
{
	char *tmp = NULL;
#define FAIL_TESTS { printf("tmp: %s\n", tmp); free(tmp); assert(0); return -1; }

	join_paths(&tmp, "/a/b/c", "d/e/../f");
	if (strcmp(tmp, "/a/b/c/d/e/../f")) FAIL_TESTS;

	expand_path(&tmp);
	if (strcmp(tmp, "/a/b/c/d/f")) FAIL_TESTS;
	free(tmp);

	join_paths(&tmp, "/folder1/folder2", "folder1/folder2/folder3");
	if (strcmp(tmp, "/folder1/folder2/folder1/folder2/folder3")) FAIL_TESTS;

	expand_path(&tmp);
	if (strcmp(tmp, "/folder1/folder2/folder1/folder2/folder3")) FAIL_TESTS;
	free(tmp);

	join_paths(&tmp, "/.(a)./b/cthing", "../../../");
	if (strcmp(tmp, "/.(a)./b/cthing/../../../")) FAIL_TESTS;

	expand_path(&tmp);
	if (strcmp(tmp, "/")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/", "/a/");
	if (strcmp(tmp, "../")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/a/test/path", "/a/test/");
	if (strcmp(tmp, "path")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/a/test/path", "/a/test");
	if (strcmp(tmp, "path")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/another/test/", "/another/test/of/paths");
	if (strcmp(tmp, "../../")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/", "/1/2/3/4/5/");
	if (strcmp(tmp, "../../../../../")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/1/2/3/", "/1/2/3/");
	if (strcmp(tmp, "./")) FAIL_TESTS;
	free(tmp);

	path_relative_to(&tmp, "/Makefile", "/Make_utils/Foo/bar");
	if (strcmp(tmp, "../../../Makefile")) FAIL_TESTS;
	free(tmp);

	if (strcmp(file_extension_from_path("id/ed.2/3"), "")) FAIL_TESTS;
	if (strcmp(file_extension_from_path("/.ssh/id_ed25519.pub"), ".pub")) FAIL_TESTS;
	if (strcmp(file_extension_from_path("/ssh/id_ed25519.pub"), ".pub")) FAIL_TESTS;
	if (strcmp(file_extension_from_path(""), "")) FAIL_TESTS;

	return 0;
}

