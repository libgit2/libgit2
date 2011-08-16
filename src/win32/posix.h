#ifndef INCLUDE_posix__w32_h__
#define INCLUDE_posix__w32_h__

#include "common.h"
#include "fnmatch.h"
#include "utf8-conv.h"

GIT_INLINE(int) p_link(const char *GIT_UNUSED(old), const char *GIT_UNUSED(new))
{
	GIT_UNUSED_ARG(old)
	GIT_UNUSED_ARG(new)
	errno = ENOSYS;
	return -1;
}

GIT_INLINE(int) p_mkdir(const char *path, int GIT_UNUSED(mode))
{
	wchar_t* buf = conv_utf8_to_utf16(path);
	int ret = _wmkdir(buf);

	GIT_UNUSED_ARG(mode)

	free(buf);
	return ret;
}

extern int p_unlink(const char *path);
extern int p_lstat(const char *file_name, struct stat *buf);
extern int p_readlink(const char *link, char *target, size_t target_len);
extern int p_hide_directory__w32(const char *path);
extern char *p_realpath(const char *orig_path, char *buffer);
extern int p_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);

extern int p_stat(const char* path, struct stat* buf);
extern int p_chdir(const char* path);
extern int p_chmod(const char* path, int mode);
extern int p_rmdir(const char* path);

#endif
