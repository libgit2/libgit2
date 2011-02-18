#ifndef INCLUDE_filebuf_h__
#define INCLUDE_filebuf_h__

#include "fileops.h"
#include "hash.h"

#ifdef GIT_THREADS
#	define GIT_FILEBUF_THREADS
#endif

#define GIT_FILEBUF_HASH_CONTENTS 0x1
#define GIT_FILEBUF_APPEND 0x2
#define GIT_FILEBUF_FORCE 0x4

#define GIT_FILELOCK_EXTENSION ".lock\0"
#define GIT_FILELOCK_EXTLENGTH 6

struct git_filebuf {
	char *path_original;
	char *path_lock;

	git_hash_ctx *digest;

	unsigned char *buffer;
#ifdef GIT_FILEBUF_THREADS
	unsigned char *buffer_back;
#endif

	size_t buf_size, buf_pos;
	git_file fd;
};

typedef struct git_filebuf git_filebuf;

int git_filebuf_write(git_filebuf *lock, void *buff, size_t len);
int git_filebuf_reserve(git_filebuf *file, void **buff, size_t len);
int git_filebuf_printf(git_filebuf *file, const char *format, ...);

int git_filebuf_open(git_filebuf *lock, const char *path, int flags);
int git_filebuf_commit(git_filebuf *lock);
void git_filebuf_cleanup(git_filebuf *lock);
int git_filebuf_hash(git_oid *oid, git_filebuf *file);

#endif
