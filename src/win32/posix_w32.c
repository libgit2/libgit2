/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "../posix.h"
#include "path.h"
#include "utf-conv.h"
#include "repository.h"
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <ws2tcpip.h>

int p_unlink(const char *path)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	_wchmod(buf, 0666);
	return _wunlink(buf);
}

int p_fsync(int fd)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	if (!FlushFileBuffers(fh)) {
		DWORD code = GetLastError();

		if (code == ERROR_INVALID_HANDLE)
			errno = EINVAL;
		else
			errno = EIO;

		return -1;
	}

	return 0;
}

GIT_INLINE(time_t) filetime_to_time_t(const FILETIME *ft)
{
	long long winTime = ((long long)ft->dwHighDateTime << 32) + ft->dwLowDateTime;
	winTime -= 116444736000000000LL; /* Windows to Unix Epoch conversion */
	winTime /= 10000000;		 /* Nano to seconds resolution */
	return (time_t)winTime;
}

#define WIN32_IS_WSEP(CH) ((CH) == L'/' || (CH) == L'\\')

static int do_lstat(
	const char *file_name, struct stat *buf, int posix_enotdir)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;
	wchar_t fbuf[GIT_WIN_PATH], lastch;
	int flen;

	flen = git__utf8_to_16(fbuf, GIT_WIN_PATH, file_name);

	/* truncate trailing slashes */
	for (; flen > 0; --flen) {
		lastch = fbuf[flen - 1];
		if (WIN32_IS_WSEP(lastch))
			fbuf[flen - 1] = L'\0';
		else if (lastch != L'\0')
			break;
	}

	if (GetFileAttributesExW(fbuf, GetFileExInfoStandard, &fdata)) {
		int fMode = S_IREAD;

		if (!buf)
			return 0;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			fMode |= S_IFDIR;
		else
			fMode |= S_IFREG;

		if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
			fMode |= S_IWRITE;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			fMode |= S_IFLNK;

		buf->st_ino = 0;
		buf->st_gid = 0;
		buf->st_uid = 0;
		buf->st_nlink = 1;
		buf->st_mode = (mode_t)fMode;
		buf->st_size = ((git_off_t)fdata.nFileSizeHigh << 32) + fdata.nFileSizeLow;
		buf->st_dev = buf->st_rdev = (_getdrive() - 1);
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));

		/* Windows symlinks have zero file size, call readlink to determine
		 * the length of the path pointed to, which we expect everywhere else
		 */
		if (S_ISLNK(fMode)) {
			char target[GIT_WIN_PATH];
			int readlink_result;

			readlink_result = p_readlink(file_name, target, GIT_WIN_PATH);

			if (readlink_result == -1)
				return -1;

			buf->st_size = strlen(target);
		}

		return 0;
	}

	errno = ENOENT;

	/* We need POSIX behavior, then ENOTDIR must set when any of the folders in the
	 * file path is a regular file,otherwise ENOENT must be set.
	 */
	if (posix_enotdir) {
		/* scan up path until we find an existing item */
		while (1) {
			/* remove last directory component */
			for (--flen; flen > 0 && !WIN32_IS_WSEP(fbuf[flen]); --flen);

			if (flen <= 0)
				break;

			fbuf[flen] = L'\0';

			if (GetFileAttributesExW(fbuf, GetFileExInfoStandard, &fdata)) {
				if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					errno = ENOTDIR;
				break;
			}
		}
	}

	return -1;
}

int p_lstat(const char *filename, struct stat *buf)
{
	return do_lstat(filename, buf, 0);
}

int p_lstat_posixly(const char *filename, struct stat *buf)
{
	return do_lstat(filename, buf, 1);
}

int p_readlink(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPWSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;
	wchar_t link_w[GIT_WIN_PATH];
	wchar_t* target_w;
	int error = 0;

	assert(link && target && target_len > 0);

	/*
	 * Try to load the pointer to pGetFinalPath dynamically, because
	 * it is not available in platforms older than Vista
	 */
	if (pGetFinalPath == NULL) {
		HMODULE module = GetModuleHandle("kernel32");

		if (module != NULL)
			pGetFinalPath = (fpath_func)GetProcAddress(module, "GetFinalPathNameByHandleW");

		if (pGetFinalPath == NULL) {
			giterr_set(GITERR_OS,
				"'GetFinalPathNameByHandleW' is not available in this platform");
			return -1;
		}
	}

	git__utf8_to_16(link_w, GIT_WIN_PATH, link);

	hFile = CreateFileW(link_w,			// file to open
			GENERIC_READ,			// open for reading
			FILE_SHARE_READ,		// share for reading
			NULL,					// default security
			OPEN_EXISTING,			// existing file only
			FILE_FLAG_BACKUP_SEMANTICS, // normal file
			NULL);					// no attr. template

	if (hFile == INVALID_HANDLE_VALUE) {
		giterr_set(GITERR_OS, "Cannot open '%s' for reading", link);
		return -1;
	}

	target_w = (wchar_t*)git__malloc(target_len * sizeof(wchar_t));
	GITERR_CHECK_ALLOC(target_w);

	dwRet = pGetFinalPath(hFile, target_w, (DWORD)target_len, 0x0);
	if (dwRet == 0 ||
		dwRet >= target_len ||
		!WideCharToMultiByte(CP_UTF8, 0, target_w, -1, target,
			(int)(target_len * sizeof(char)), NULL, NULL))
		error = -1;

	git__free(target_w);
	CloseHandle(hFile);

	if (error)
		return error;

	/* Skip first 4 characters if they are "\\?\" */
	if (dwRet > 4 &&
		target[0] == '\\' && target[1] == '\\' &&
		target[2] == '?' && target[3] == '\\')
	{
		unsigned int offset = 4;
		dwRet -= 4;

		/* \??\UNC\ */
		if (dwRet > 7 &&
			target[4] == 'U' && target[5] == 'N' && target[6] == 'C')
		{
			offset += 2;
			dwRet -= 2;
			target[offset] = '\\';
		}

		memmove(target, target + offset, dwRet);
	}

	target[dwRet] = '\0';

	return dwRet;
}

int p_symlink(const char *old, const char *new)
{
	/* Real symlinks on NTFS require admin privileges. Until this changes,
	 * libgit2 just creates a text file with the link target in the contents.
	 */
	return git_futils_fake_symlink(old, new);
}

int p_open(const char *path, int flags, ...)
{
	wchar_t buf[GIT_WIN_PATH];
	mode_t mode = 0;

	git__utf8_to_16(buf, GIT_WIN_PATH, path);

	if (flags & O_CREAT) {
		va_list arg_list;

		va_start(arg_list, flags);
		mode = (mode_t)va_arg(arg_list, int);
		va_end(arg_list);
	}

	return _wopen(buf, flags | _O_BINARY, mode);
}

int p_creat(const char *path, mode_t mode)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return _wopen(buf, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, mode);
}

int p_getcwd(char *buffer_out, size_t size)
{
	int ret;
	wchar_t *buf;

	if ((size_t)((int)size) != size)
		return -1;

	buf = (wchar_t*)git__malloc(sizeof(wchar_t) * (int)size);
	GITERR_CHECK_ALLOC(buf);

	_wgetcwd(buf, (int)size);

	ret = WideCharToMultiByte(
		CP_UTF8, 0, buf, -1, buffer_out, (int)size, NULL, NULL);

	git__free(buf);
	return !ret ? -1 : 0;
}

int p_stat(const char* path, struct stat* buf)
{
	return do_lstat(path, buf, 0);
}

int p_chdir(const char* path)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return _wchdir(buf);
}

int p_chmod(const char* path, mode_t mode)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return _wchmod(buf, mode);
}

int p_rmdir(const char* path)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return _wrmdir(buf);
}

int p_hide_directory__w32(const char *path)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return (SetFileAttributesW(buf, FILE_ATTRIBUTE_HIDDEN) != 0) ? 0 : -1;
}

char *p_realpath(const char *orig_path, char *buffer)
{
	int ret;
	wchar_t orig_path_w[GIT_WIN_PATH];
	wchar_t buffer_w[GIT_WIN_PATH];

	git__utf8_to_16(orig_path_w, GIT_WIN_PATH, orig_path);

	/* Implicitly use GetCurrentDirectory which can be a threading issue */
	ret = GetFullPathNameW(orig_path_w, GIT_WIN_PATH, buffer_w, NULL);

	/* According to MSDN, a return value equals to zero means a failure. */
	if (ret == 0 || ret > GIT_WIN_PATH)
		buffer = NULL;

	else if (GetFileAttributesW(buffer_w) == INVALID_FILE_ATTRIBUTES) {
		buffer = NULL;
		errno = ENOENT;
	}

	else if (buffer == NULL) {
		int buffer_sz = WideCharToMultiByte(
			CP_UTF8, 0, buffer_w, -1, NULL, 0, NULL, NULL);

		if (!buffer_sz ||
			!(buffer = (char *)git__malloc(buffer_sz)) ||
			!WideCharToMultiByte(
				CP_UTF8, 0, buffer_w, -1, buffer, buffer_sz, NULL, NULL))
		{
			git__free(buffer);
			buffer = NULL;
		}
	}

	else if (!WideCharToMultiByte(
		CP_UTF8, 0, buffer_w, -1, buffer, GIT_PATH_MAX, NULL, NULL))
		buffer = NULL;

	if (buffer)
		git_path_mkposix(buffer);

	return buffer;
}

int p_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
#ifdef _MSC_VER
	int len;

	if (count == 0 ||
		(len = _vsnprintf_s(buffer, count, _TRUNCATE, format, argptr)) < 0)
		return _vscprintf(format, argptr);

	return len;
#else /* MinGW */
	return vsnprintf(buffer, count, format, argptr);
#endif
}

int p_snprintf(char *buffer, size_t count, const char *format, ...)
{
	va_list va;
	int r;

	va_start(va, format);
	r = p_vsnprintf(buffer, count, format, va);
	va_end(va);

	return r;
}

extern int p_creat(const char *path, mode_t mode);

int p_mkstemp(char *tmp_path)
{
#if defined(_MSC_VER)
	if (_mktemp_s(tmp_path, strlen(tmp_path) + 1) != 0)
		return -1;
#else
	if (_mktemp(tmp_path) == NULL)
		return -1;
#endif

	return p_creat(tmp_path, 0744); //-V536
}

int p_setenv(const char* name, const char* value, int overwrite)
{
	if (overwrite != 1)
		return -1;

	return (SetEnvironmentVariableA(name, value) == 0 ? -1 : 0);
}

int p_access(const char* path, mode_t mode)
{
	wchar_t buf[GIT_WIN_PATH];
	git__utf8_to_16(buf, GIT_WIN_PATH, path);
	return _waccess(buf, mode);
}

int p_rename(const char *from, const char *to)
{
	wchar_t wfrom[GIT_WIN_PATH];
	wchar_t wto[GIT_WIN_PATH];

	git__utf8_to_16(wfrom, GIT_WIN_PATH, from);
	git__utf8_to_16(wto, GIT_WIN_PATH, to);
	return MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) ? 0 : -1;
}

int p_recv(GIT_SOCKET socket, void *buffer, size_t length, int flags)
{
	if ((size_t)((int)length) != length)
		return -1; /* giterr_set will be done by caller */

	return recv(socket, buffer, (int)length, flags);
}

int p_send(GIT_SOCKET socket, const void *buffer, size_t length, int flags)
{
	if ((size_t)((int)length) != length)
		return -1; /* giterr_set will be done by caller */

	return send(socket, buffer, (int)length, flags);
}

/**
 * Borrowed from http://old.nabble.com/Porting-localtime_r-and-gmtime_r-td15282276.html
 * On Win32, `gmtime_r` doesn't exist but `gmtime` is threadsafe, so we can use that
 */
struct tm * 
p_localtime_r (const time_t *timer, struct tm *result) 
{ 
   struct tm *local_result; 
   local_result = localtime (timer); 

   if (local_result == NULL || result == NULL) 
      return NULL; 

   memcpy (result, local_result, sizeof (struct tm)); 
   return result; 
} 
struct tm * 
p_gmtime_r (const time_t *timer, struct tm *result) 
{ 
   struct tm *local_result; 
   local_result = gmtime (timer); 

   if (local_result == NULL || result == NULL) 
      return NULL; 

   memcpy (result, local_result, sizeof (struct tm)); 
   return result; 
}

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 
#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone 
{
   int  tz_minuteswest; /* minutes W of Greenwich */
   int  tz_dsttime;     /* type of dst correction */
};
#endif
 
int p_gettimeofday(struct timeval *tv, struct timezone *tz)
{
   FILETIME ft;
   unsigned __int64 tmpres = 0;
   static int tzflag;
 
   if (NULL != tv)
      {
         GetSystemTimeAsFileTime(&ft);
 
         tmpres |= ft.dwHighDateTime;
         tmpres <<= 32;
         tmpres |= ft.dwLowDateTime;
 
         /*converting file time to unix epoch*/
         tmpres /= 10;  /*convert into microseconds*/
         tmpres -= DELTA_EPOCH_IN_MICROSECS; 
         tv->tv_sec = (long)(tmpres / 1000000UL);
         tv->tv_usec = (long)(tmpres % 1000000UL);
      }
 
   if (NULL != tz)
      {
         if (!tzflag)
            {
               _tzset();
               tzflag++;
            }
         tz->tz_minuteswest = _timezone / 60;
         tz->tz_dsttime = _daylight;
      }
 
   return 0;
}

int p_inet_pton(int af, const char* src, void* dst)
{
	union {
		struct sockaddr_in6 sin6;
		struct sockaddr_in sin;
	} sa;
	int srcsize;

	switch(af)
	{
		case AF_INET:
			sa.sin.sin_family = AF_INET;
			srcsize = (int)sizeof(sa.sin);
		break;
		case AF_INET6:
			sa.sin6.sin6_family = AF_INET6;
			srcsize = (int)sizeof(sa.sin6);
		break;
		default:
			errno = WSAEPFNOSUPPORT;
			return -1;
	}

	if (WSAStringToAddress((LPSTR)src, af, NULL, (struct sockaddr *) &sa, &srcsize) != 0)
	{
		errno = WSAGetLastError();
		return -1;
	}

	switch(af)
	{
		case AF_INET:
			memcpy(dst, &sa.sin.sin_addr, sizeof(sa.sin.sin_addr));
		break;
		case AF_INET6:
			memcpy(dst, &sa.sin6.sin6_addr, sizeof(sa.sin6.sin6_addr));
		break;
	}

	return 1;
}
