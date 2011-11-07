/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_posix__w32_h__
#define INCLUDE_posix__w32_h__

#include "common.h"
#include "fnmatch.h"
#include "utf-conv.h"

GIT_INLINE(int) p_link(const char *GIT_UNUSED(old), const char *GIT_UNUSED(new))
{
	GIT_UNUSED_ARG(old)
	GIT_UNUSED_ARG(new)
	errno = ENOSYS;
	return -1;
}

GIT_INLINE(int) p_mkdir(const char *path, mode_t GIT_UNUSED(mode))
{
	wchar_t* buf = gitwin_to_utf16(path);
	int ret = _wmkdir(buf);

	GIT_UNUSED_ARG(mode)

	git__free(buf);
	return ret;
}

extern int p_unlink(const char *path);
extern int p_lstat(const char *file_name, struct stat *buf);
extern int p_readlink(const char *link, char *target, size_t target_len);
extern int p_hide_directory__w32(const char *path);
extern char *p_realpath(const char *orig_path, char *buffer);
extern int p_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr);
extern int p_snprintf(char *buffer, size_t count, const char *format, ...) GIT_FORMAT_PRINTF(3, 4);
extern int p_mkstemp(char *tmp_path);
extern int p_setenv(const char* name, const char* value, int overwrite);
extern int p_stat(const char* path, struct stat* buf);
extern int p_chdir(const char* path);
extern int p_chmod(const char* path, mode_t mode);
extern int p_rmdir(const char* path);
extern int p_access(const char* path, mode_t mode);
extern int p_fsync(int fd);
extern int p_open(const char *path, int flags);
extern int p_creat(const char *path, mode_t mode);
extern int p_getcwd(char *buffer_out, size_t size);
extern int p_rename(const char *from, const char *to);

#endif
