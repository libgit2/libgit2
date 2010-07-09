#ifndef INCLUDE_filelock_h__
#define INCLUDE_filelock_h__

#include "fileops.h"

struct git_filelock {

	char path[GIT_PATH_MAX];
	size_t path_length;

	git_file file_lock;
	int is_locked;
};

typedef struct git_filelock git_filelock;

int git_filelock_init(git_filelock *lock, const char *path);
int git_filelock_lock(git_filelock *lock, int append);
void git_filelock_unlock(git_filelock *lock);
int git_filelock_commit(git_filelock *lock);
int git_filelock_write(git_filelock *lock, const void *buffer, size_t length);

#endif
