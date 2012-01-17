/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_dir_h__
#define INCLUDE_dir_h__

#include "common.h"

struct git__dirent {
	int d_ino;
	char d_name[261];
};

typedef struct {
	HANDLE h;
	WIN32_FIND_DATAW f;
	struct git__dirent entry;
	char *dir;
	int first;
} git__DIR;

extern git__DIR *git__opendir(const char *);
extern struct git__dirent *git__readdir(git__DIR *);
extern void git__rewinddir(git__DIR *);
extern int git__closedir(git__DIR *);

# ifndef GIT__WIN32_NO_WRAP_DIR
#	define dirent git__dirent
#	define DIR git__DIR
#	define opendir	git__opendir
#	define readdir	git__readdir
#	define rewinddir git__rewinddir
#	define closedir git__closedir
# endif

#endif /* INCLUDE_dir_h__ */
