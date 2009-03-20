#ifndef INCLUDE_dir_h__
#define INCLUDE_dir_h__

#include "common.h"

#ifndef GIT_WIN32
# include <dirent.h>
#endif

#ifdef GIT_WIN32

struct dirent {
	int  d_ino;
	char d_name[261];
};

typedef struct {
	HANDLE h;
	WIN32_FIND_DATA f;
	struct dirent entry;
	char *dir;
	int first;
} DIR;

extern DIR *opendir(const char *);
extern struct dirent *readdir(DIR *);
extern void rewinddir(DIR *);
extern int closedir(DIR *);

#endif

#endif /* INCLUDE_dir_h__ */
