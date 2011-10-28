/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#define GIT__WIN32_NO_WRAP_DIR
#include "dir.h"
#include "utf-conv.h"
#include "git2/windows.h"

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
	char filter[4096];
	wchar_t* filter_w;
	git__DIR *new;

	if (!dir || !init_filter(filter, sizeof(filter), dir))
		return NULL;

	new = git__malloc(sizeof(*new));
	if (!new)
		return NULL;

	new->dir = git__malloc(strlen(dir)+1);
	if (!new->dir) {
		git__free(new);
		return NULL;
	}
	strcpy(new->dir, dir);

	filter_w = gitwin_to_utf16(filter);
	new->h = FindFirstFileW(filter_w, &new->f);
	git__free(filter_w);

	if (new->h == INVALID_HANDLE_VALUE) {
		git__free(new->dir);
		git__free(new);
		return NULL;
	}
	new->first = 1;

	return new;
}

struct git__dirent *git__readdir(git__DIR *d)
{
	if (!d || d->h == INVALID_HANDLE_VALUE)
		return NULL;

	if (d->first)
		d->first = 0;
	else {
		if (!FindNextFileW(d->h, &d->f))
			return NULL;
	}

	if (wcslen(d->f.cFileName) >= sizeof(d->entry.d_name))
		return NULL;

	d->entry.d_ino = 0;
	WideCharToMultiByte(gitwin_get_codepage(), 0, d->f.cFileName, -1, d->entry.d_name, GIT_PATH_MAX, NULL, NULL);

	return &d->entry;
}

void git__rewinddir(git__DIR *d)
{
	char filter[4096];
	wchar_t* filter_w;

	if (d) {
		if (d->h != INVALID_HANDLE_VALUE)
			FindClose(d->h);
		d->h = INVALID_HANDLE_VALUE;
		d->first = 0;

		if (init_filter(filter, sizeof(filter), d->dir)) {
			filter_w = gitwin_to_utf16(filter);
			d->h = FindFirstFileW(filter_w, &d->f);
			git__free(filter_w);

			if (d->h != INVALID_HANDLE_VALUE)
				d->first = 1;
		}
	}
}

int git__closedir(git__DIR *d)
{
	if (d) {
		if (d->h != INVALID_HANDLE_VALUE)
			FindClose(d->h);
		if (d->dir)
			git__free(d->dir);
		git__free(d);
	}
	return 0;
}

