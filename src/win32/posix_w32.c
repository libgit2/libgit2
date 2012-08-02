/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "../posix.h"
#include "path.h"
#include "utf-conv.h"
#include <errno.h>
#include <io.h>
#include <fcntl.h>


int p_unlink(const char *path)
{
	int ret;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;

	_wchmod(gitwin_path_ptr(winpath), 0666);
	ret = _wunlink(gitwin_path_ptr(winpath));
	gitwin_path_free(winpath);

	return ret;
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

static int do_lstat(const char *file_name, struct stat *buf)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;
	DWORD last_error;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, file_name, strlen(file_name)) < 0)
		return -1;

	if (GetFileAttributesExW(gitwin_path_ptr(winpath), GetFileExInfoStandard, &fdata)) {
		int fMode = S_IREAD;

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

		gitwin_path_free(winpath);
		return 0;
	}

	last_error = GetLastError();
	if (last_error == ERROR_FILE_NOT_FOUND)
		errno = ENOENT;
	else if (last_error == ERROR_PATH_NOT_FOUND)
		errno = ENOTDIR;

	gitwin_path_free(winpath);
	return -1;
}

int p_lstat(const char *file_name, struct stat *buf)
{
	int error;
	size_t namelen;
	char *alt_name;

	if (do_lstat(file_name, buf) == 0)
		return 0;

	/* if file_name ended in a '/', Windows returned ENOENT;
	 * try again without trailing slashes
	 */
	namelen = strlen(file_name);
	if (namelen && file_name[namelen-1] != '/')
		return -1;

	while (namelen && file_name[namelen-1] == '/')
		--namelen;

	if (!namelen)
		return -1;

	alt_name = git__strndup(file_name, namelen);
	if (!alt_name)
		return -1;

	error = do_lstat(alt_name, buf);

	git__free(alt_name);
	return error;
}

int p_readlink(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPWSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;
	gitwin_utf16_path *winpath;
	wchar_t* target_w;
	int error = 0;

	assert(link && target && target_len > 0);

	/*
	 * Try to load the pointer to pGetFinalPath dynamically, because
	 * it is not available in platforms older than Vista
	 */
	if (pGetFinalPath == NULL) {
		HINSTANCE library = LoadLibrary("kernel32");

		if (library != NULL)
			pGetFinalPath = (fpath_func)GetProcAddress(library, "GetFinalPathNameByHandleW");

		if (pGetFinalPath == NULL) {
			giterr_set(GITERR_OS,
				"'GetFinalPathNameByHandleW' is not available in this platform");
			return -1;
		}
	}

	if (gitwin_path_create(&winpath, link, strlen(link)) < 0)
		return -1;

	hFile = CreateFileW(gitwin_path_ptr(winpath),	// file to open
			GENERIC_READ,			// open for reading
			FILE_SHARE_READ,		// share for reading
			NULL,					// default security
			OPEN_EXISTING,			// existing file only
			FILE_FLAG_BACKUP_SEMANTICS, // normal file
			NULL);					// no attr. template

	gitwin_path_free(winpath);

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

int p_open(const char *path, int flags, ...)
{
	int fd;
	gitwin_utf16_path* winpath;
	mode_t mode = 0;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;

	if (flags & O_CREAT)
	{
		va_list arg_list;

		va_start(arg_list, flags);
		mode = (mode_t)va_arg(arg_list, int);
		va_end(arg_list);
	}

	fd = _wopen(gitwin_path_ptr(winpath), flags | _O_BINARY, mode);

	gitwin_path_free(winpath);
	return fd;
}

int p_creat(const char *path, mode_t mode)
{
	int fd;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;

	fd = _wopen(gitwin_path_ptr(winpath), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, mode);
	gitwin_path_free(winpath);
	return fd;
}

int p_getcwd(char *buffer_out, size_t size)
{
	int ret;
	wchar_t* buf;

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
	return do_lstat(path, buf);
}

int p_chdir(const char* path)
{
	int ret;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;
	ret = _wchdir(gitwin_path_ptr(winpath));
	gitwin_path_free(winpath);
	return ret;
}

int p_chmod(const char* path, mode_t mode)
{
	int ret;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;
	ret = _wchmod(gitwin_path_ptr(winpath), mode);
	gitwin_path_free(winpath);
	return ret;
}

int p_rmdir(const char* path)
{
	int ret;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;
	ret = _wrmdir(gitwin_path_ptr(winpath));
	gitwin_path_free(winpath);
	return ret;
}

int p_hide_directory__w32(const char *path)
{
	int res;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;

	res = SetFileAttributesW(gitwin_path_ptr(winpath), FILE_ATTRIBUTE_HIDDEN);
	gitwin_path_free(winpath);

	return (res != 0) ? 0 : -1; /* MSDN states a "non zero" value indicates a success */
}

char *p_realpath(const char *orig_path, char *buffer)
{
	int ret, buffer_sz = 0;
	gitwin_utf16_path* winpath;
	wchar_t* buffer_w = (wchar_t*)git__malloc(GIT_PATH_MAX * sizeof(wchar_t));

	if (!buffer_w)
		return NULL;

	if (gitwin_path_create(&winpath, orig_path, strlen(orig_path)) < 0)
		return NULL;

	ret = GetFullPathNameW(gitwin_path_ptr(winpath), GIT_PATH_MAX, buffer_w, NULL);
	gitwin_path_free(winpath);

	/* According to MSDN, a return value equals to zero means a failure. */
	if (ret == 0 || ret > GIT_PATH_MAX) {
		buffer = NULL;
		goto done;
	}

	if (buffer == NULL) {
		buffer_sz = WideCharToMultiByte(CP_UTF8, 0, buffer_w, -1, NULL, 0, NULL, NULL);

		if (!buffer_sz ||
			!(buffer = (char *)git__malloc(buffer_sz)) ||
			!WideCharToMultiByte(CP_UTF8, 0, buffer_w, -1, buffer, buffer_sz, NULL, NULL))
		{
			git__free(buffer);
			buffer = NULL;
			goto done;
		}
	} else {
		if (!WideCharToMultiByte(CP_UTF8, 0, buffer_w, -1, buffer, GIT_PATH_MAX, NULL, NULL)) {
			buffer = NULL;
			goto done;
		}
	}

	if (!git_path_exists(buffer))
	{
		if (buffer_sz > 0)
			git__free(buffer);

		buffer = NULL;
		errno = ENOENT;
	}

done:
	git__free(buffer_w);
	if (buffer)
		git_path_mkposix(buffer);
	return buffer;
}

int p_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
#ifdef _MSC_VER
	int len;

	if (count == 0 || (len = _vsnprintf(buffer, count, format, argptr)) < 0)
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
	int ret;
	gitwin_utf16_path* winpath;

	if (gitwin_path_create(&winpath, path, strlen(path)) < 0)
		return -1;

	ret = _waccess(gitwin_path_ptr(winpath), mode);
	gitwin_path_free(winpath);

	return ret;
}

int p_rename(const char *from, const char *to)
{
	gitwin_utf16_path *wfrom;
	gitwin_utf16_path *wto;
	int ret;

	if (gitwin_path_create(&wfrom, from, strlen(from)) < 0)
		return -1;

	if (gitwin_path_create(&wto, to, strlen(to)) < 0) {
		gitwin_path_free(wfrom);
		return -1;
	}

	ret = MoveFileExW(gitwin_path_ptr(wfrom), gitwin_path_ptr(wto),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) ? 0 : -1;

	gitwin_path_free(wfrom);
	gitwin_path_free(wto);

	return ret;
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
 
struct timezone 
{
   int  tz_minuteswest; /* minutes W of Greenwich */
   int  tz_dsttime;     /* type of dst correction */
};
 
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
