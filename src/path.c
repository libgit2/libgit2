#include "common.h"
#include "path.h"
#include "posix.h"

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git_path_basename_r(char *buffer, size_t bufflen, const char *path)
{
	const char *endp, *startp;
	int len, result;

	/* Empty or NULL string gets treated as "." */
	if (path == NULL || *path == '\0') {
		startp  = ".";
		len     = 1;
		goto Exit;
	}

	/* Strip trailing slashes */
	endp = path + strlen(path) - 1;
	while (endp > path && *endp == '/')
		endp--;

	/* All slashes becomes "/" */
	if (endp == path && *endp == '/') {
		startp = "/";
		len    = 1;
		goto Exit;
	}

	/* Find the start of the base */
	startp = endp;
	while (startp > path && *(startp - 1) != '/')
		startp--;

	len = endp - startp +1;

Exit:
	result = len;
	if (buffer == NULL) {
		return result;
	}
	if (len > (int)bufflen-1) {
		len    = (int)bufflen-1;
		result = GIT_ENOMEM;
	}

	if (len >= 0) {
		memmove(buffer, startp, len);
		buffer[len] = 0;
	}
	return result;
}

/*
 * Based on the Android implementation, BSD licensed.
 * Check http://android.git.kernel.org/
 */
int git_path_dirname_r(char *buffer, size_t bufflen, const char *path)
{
    const char *endp;
    int result, len;

    /* Empty or NULL string gets treated as "." */
    if (path == NULL || *path == '\0') {
        path = ".";
        len  = 1;
        goto Exit;
    }

    /* Strip trailing slashes */
    endp = path + strlen(path) - 1;
    while (endp > path && *endp == '/')
        endp--;

    /* Find the start of the dir */
    while (endp > path && *endp != '/')
        endp--;

    /* Either the dir is "/" or there are no slashes */
    if (endp == path) {
        path = (*endp == '/') ? "/" : ".";
        len  = 1;
        goto Exit;
    }

    do {
        endp--;
    } while (endp > path && *endp == '/');

    len = endp - path +1;

Exit:
    result = len;
    if (len+1 > GIT_PATH_MAX) {
        return GIT_ENOMEM;
    }
    if (buffer == NULL)
        return result;

    if (len > (int)bufflen-1) {
        len    = (int)bufflen-1;
        result = GIT_ENOMEM;
    }

    if (len >= 0) {
        memmove(buffer, path, len);
        buffer[len] = 0;
    }
    return result;
}


char *git_path_dirname(const char *path)
{
    char *dname = NULL;
    int len;

	len = (path ? strlen(path) : 0) + 2;
	dname = (char *)git__malloc(len);
	if (dname == NULL)
		return NULL;

    if (git_path_dirname_r(dname, len, path) < GIT_SUCCESS) {
		free(dname);
		return NULL;
	}

    return dname;
}

char *git_path_basename(const char *path)
{
    char *bname = NULL;
    int len;

	len = (path ? strlen(path) : 0) + 2;
	bname = (char *)git__malloc(len);
	if (bname == NULL)
		return NULL;

    if (git_path_basename_r(bname, len, path) < GIT_SUCCESS) {
		free(bname);
		return NULL;
	}

    return bname;
}


const char *git_path_topdir(const char *path)
{
	size_t len;
	int i;

	assert(path);
	len = strlen(path);

	if (!len || path[len - 1] != '/')
		return NULL;

	for (i = len - 2; i >= 0; --i)
		if (path[i] == '/')
			break;

	return &path[i + 1];
}

void git_path_join_n(char *buffer_out, int count, ...)
{
	va_list ap;
	int i;
	char *buffer_start = buffer_out;

	va_start(ap, count);
	for (i = 0; i < count; ++i) {
		const char *path;
		int len;

		path = va_arg(ap, const char *);

		assert((i == 0) || path != buffer_start);

		if (i > 0 && *path == '/' && buffer_out > buffer_start && buffer_out[-1] == '/')
			path++;

		if (!*path)
			continue;

		len = strlen(path);
		memmove(buffer_out, path, len);
		buffer_out = buffer_out + len;

		if (i < count - 1 && buffer_out[-1] != '/')
			*buffer_out++ = '/';
	}
	va_end(ap);

	*buffer_out = '\0';
}

int git_path_root(const char *path)
{
	int offset = 0;

#ifdef GIT_WIN32
	/* Does the root of the path look like a windows drive ? */
	if (isalpha(path[0]) && (path[1] == ':'))
		offset += 2;
#endif

	if (*(path + offset) == '/')
		return offset;

	return -1;	/* Not a real error. Rather a signal than the path is not rooted */
}

int git_path_prettify(char *path_out, const char *path, const char *base)
{
	char *result;

	if (base == NULL || git_path_root(path) >= 0) {
		result = p_realpath(path, path_out);
	} else {
		char aux_path[GIT_PATH_MAX];
		git_path_join(aux_path, base, path);
		result = p_realpath(aux_path, path_out);
	}

	return result ? GIT_SUCCESS : GIT_EOSERR;
}

int git_path_prettify_dir(char *path_out, const char *path, const char *base)
{
	size_t end;

	if (git_path_prettify(path_out, path, base) < GIT_SUCCESS)
		return GIT_EOSERR;

	end = strlen(path_out);

	if (end && path_out[end - 1] != '/') {
		path_out[end] = '/';
		path_out[end + 1] = '\0';
	}

	return GIT_SUCCESS;
}
