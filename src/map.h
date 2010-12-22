#ifndef INCLUDE_map_h__
#define INCLUDE_map_h__

#include "common.h"


/* git__mmap() prot values */
#define GIT_PROT_NONE  0x0
#define GIT_PROT_READ  0x1
#define GIT_PROT_WRITE 0x2
#define GIT_PROT_EXEC  0x4

/* git__mmmap() flags values */
#define GIT_MAP_FILE    0
#define GIT_MAP_SHARED  1
#define GIT_MAP_PRIVATE 2
#define GIT_MAP_TYPE    0xf
#define GIT_MAP_FIXED   0x10

typedef struct {  /* memory mapped buffer   */
	void *data;  /* data bytes          */
	size_t len;  /* data length         */
#ifdef GIT_WIN32
	HANDLE fmh;  /* file mapping handle */
#endif
} git_map;

extern int git__mmap(git_map *out, size_t len, int prot, int flags, int fd, off_t offset);
extern int git__munmap(git_map *map);

#endif /* INCLUDE_map_h__ */
