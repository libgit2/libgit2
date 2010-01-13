#define GIT__WIN32_NO_WRAP_DIR
#include "dir.h"

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
	git__DIR *new;

	if (!dir || !init_filter(filter, sizeof(filter), dir))
		return NULL;

	new = git__malloc(sizeof(*new));
	if (!new)
		return NULL;

	new->dir = git__malloc(strlen(dir)+1);
	if (!new->dir) {
		free(new);
		return NULL;
	}
	strcpy(new->dir, dir);

	new->h = FindFirstFile(filter, &new->f);
	if (new->h == INVALID_HANDLE_VALUE) {
		free(new->dir);
		free(new);
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
		if (!FindNextFile(d->h, &d->f))
			return NULL;
	}

	if (strlen(d->f.cFileName) >= sizeof(d->entry.d_name))
		return NULL;

	d->entry.d_ino = 0;
	strcpy(d->entry.d_name, d->f.cFileName);

	return &d->entry;
}

void git__rewinddir(git__DIR *d)
{
	char filter[4096];

	if (d) {
		if (d->h != INVALID_HANDLE_VALUE)
			FindClose(d->h);
		d->h = INVALID_HANDLE_VALUE;
		d->first = 0;
		if (init_filter(filter, sizeof(filter), d->dir)) {
			d->h = FindFirstFile(filter, &d->f);
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
			free(d->dir);
		free(d);
	}
	return 0;
}

