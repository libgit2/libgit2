#ifndef INCLUDE_buffer_h__
#define INCLUDE_buffer_h__

#include "common.h"

typedef struct {
	char *ptr;
	ssize_t asize, size;
} git_buf;

#define GIT_BUF_INIT {NULL, 0, 0}

int git_buf_grow(git_buf *buf, size_t target_size);
int git_buf_oom(const git_buf *buf);
void git_buf_putc(git_buf *buf, char c);
void git_buf_put(git_buf *buf, const char *data, size_t len);
void git_buf_puts(git_buf *buf, const char *string);
void git_buf_printf(git_buf *buf, const char *format, ...) GIT_FORMAT_PRINTF(2, 3);
const char *git_buf_cstr(git_buf *buf);
void git_buf_free(git_buf *buf);

#define git_buf_PUTS(buf, str) git_buf_put(buf, str, sizeof(str) - 1)

#endif
