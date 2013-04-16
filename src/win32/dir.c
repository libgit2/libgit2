/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#define GIT__WIN32_NO_WRAP_DIR
#include "dir.h"
#include "utf-conv.h"

static int init_filter(char *filter, size_t n, const char *dir)
{
	size_t len = strlen(dir);

	if (len+3 >= n)
		return 0;

	strcpy(filter, dir);
	if (len && dir[len-1] != '/')
		strcat(filter, "/");
	strcat(filter, "*");

	return 1;
}

git__DIR *git__opendir(const char *dir)
{
	char filter[GIT_WIN_PATH];
	wchar_t filter_w[GIT_WIN_PATH];
	git__DIR *new = NULL;

	if (!dir || !init_filter(filter, sizeof(filter), dir))
		return NULL;

	new = git__malloc(sizeof(*new));
	if (!new)
		return NULL;

	new->dir = git__strdup(dir);
	if (!new->dir)
		goto fail;

	git__utf8_to_16(filter_w, GIT_WIN_PATH, filter);
	new->h = FindFirstFileW(filter_w, &new->f);

	if (new->h == INVALID_HANDLE_VALUE) {
		giterr_set(GITERR_OS, "Could not open directory '%s'", dir);
		goto fail;
	}

	new->first = 1;
	return new;

fail:
	git__free(new->dir);
	git__free(new);
	return NULL;
}

int git__readdir_ext(
	git__DIR *d,
	struct git__dirent *entry,
	struct git__dirent **result,
	int *is_dir)
{
	if (!d || !entry || !result || d->h == INVALID_HANDLE_VALUE)
		return -1;

	*result = NULL;

	if (d->first)
		d->first = 0;
	else if (!FindNextFileW(d->h, &d->f)) {
		if (GetLastError() == ERROR_NO_MORE_FILES)
			return 0;
		giterr_set(GITERR_OS, "Could not read from directory '%s'", d->dir);
		return -1;
	}

	if (wcslen(d->f.cFileName) >= sizeof(entry->d_name))
		return -1;

	git__utf16_to_8(entry->d_name, d->f.cFileName);
	entry->d_ino = 0;

	*result = entry;

	if (is_dir != NULL)
		*is_dir = ((d->f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

	return 0;
}

struct git__dirent *git__readdir(git__DIR *d)
{
	struct git__dirent *result;
	if (git__readdir_ext(d, &d->entry, &result, NULL) < 0)
		return NULL;
	return result;
}

void git__rewinddir(git__DIR *d)
{
	char filter[GIT_WIN_PATH];
	wchar_t filter_w[GIT_WIN_PATH];

	if (!d)
		return;

	if (d->h != INVALID_HANDLE_VALUE) {
		FindClose(d->h);
		d->h = INVALID_HANDLE_VALUE;
		d->first = 0;
	}

	if (!init_filter(filter, sizeof(filter), d->dir))
		return;

	git__utf8_to_16(filter_w, GIT_WIN_PATH, filter);
	d->h = FindFirstFileW(filter_w, &d->f);

	if (d->h == INVALID_HANDLE_VALUE)
		giterr_set(GITERR_OS, "Could not open directory '%s'", d->dir);
	else
		d->first = 1;
}

int git__closedir(git__DIR *d)
{
	if (!d)
		return 0;

	if (d->h != INVALID_HANDLE_VALUE) {
		FindClose(d->h);
		d->h = INVALID_HANDLE_VALUE;
	}
	git__free(d->dir);
	d->dir = NULL;
	git__free(d);
	return 0;
}

