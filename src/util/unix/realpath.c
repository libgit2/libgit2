/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2_util.h"

#ifndef GIT_WIN32

#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __wasi__
# include <errno.h>
# include <string.h>
# include <sys/stat.h>

/*
 * wasi-libc's realpath() calls readlink() on every prefix of the path, and
 * a prefix that is not inside a preopened directory fails: "/a" for the
 * preopen "/a/b", so that nothing below a preopen of more than one path
 * component resolves. Resolve such a path lexically instead (`.` and `..`,
 * relative to the working directory; symbolic links are left as they are),
 * if it exists.
 */
static char *wasi_lexical_realpath(const char *pathname, char *resolved)
{
	char buf[PATH_MAX], out[PATH_MAX];
	size_t q = 0;
	const char *p, *e;
	struct stat st;

	if (pathname[0] == '/') {
		if (strlen(pathname) >= sizeof(buf)) goto toolong;
		strcpy(buf, pathname);
	} else {
		if (!getcwd(buf, sizeof(buf))) return NULL;
		if (strlen(buf) + 1 + strlen(pathname) >= sizeof(buf)) goto toolong;
		strcat(buf, "/");
		strcat(buf, pathname);
	}

	for (p = buf; *p; p = e) {
		size_t l;
		while (*p == '/') p++;
		for (e = p; *e && *e != '/'; e++);
		l = (size_t)(e - p);
		if (l == 0 || (l == 1 && p[0] == '.'))
			continue;
		if (l == 2 && p[0] == '.' && p[1] == '.') {
			while (q > 0 && out[q - 1] != '/') q--;
			if (q > 0) q--;
			continue;
		}
		if (q + 1 + l >= sizeof(out)) goto toolong;
		out[q++] = '/';
		memcpy(out + q, p, l);
		q += l;
	}
	if (q == 0) out[q++] = '/';
	out[q] = '\0';

	if (stat(out, &st) < 0)
		return NULL;

	if (!resolved)
		return git__strdup(out);
	memcpy(resolved, out, q + 1);
	return resolved;

toolong:
	errno = ENAMETOOLONG;
	return NULL;
}
#endif

char *p_realpath(const char *pathname, char *resolved)
{
	char *result;

	if ((result = realpath(pathname, resolved)) == NULL) {
#ifdef __wasi__
		if (errno == ENOENT || errno == ENOTCAPABLE)
			return wasi_lexical_realpath(pathname, resolved);
#endif
		return NULL;
	}

#ifdef __OpenBSD__
	/* The OpenBSD realpath function behaves differently,
	 * figure out if the file exists */
	if (access(result, F_OK) < 0) {
		if (!resolved)
			free(result);

		return NULL;
	}
#endif

	/*
	 * If resolved == NULL, the system has allocated the result
	 * string. We need to strdup this into _our_ allocator pool
	 * so that callers can free it with git__free.
	 */
	if (!resolved) {
		char *dup = git__strdup(result);
		free(result);

		result = dup;
	}

	return result;
}

#endif
