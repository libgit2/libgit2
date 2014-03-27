/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "../posix.h"
#include "../fileops.h"
#include "path.h"
#include "utf-conv.h"
#include "repository.h"
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <ws2tcpip.h>

int p_unlink(const char *path)
{
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
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
	git_win32_path fbuf;
	wchar_t lastch;
	int flen;

	flen = git_win32_path_from_c(fbuf, file_name);

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

		if ((fMode & (S_IFDIR | S_IFLNK)) == (S_IFDIR | S_IFLNK)) // junction
			fMode ^= S_IFLNK;

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
			git_win32_path_as_utf8 target;
			int readlink_result;

			readlink_result = p_readlink(file_name, target, sizeof(target));

			if (readlink_result == -1)
				return -1;

			buf->st_size = strlen(target);
		}

		return 0;
	}

	errno = ENOENT;

	/* To match POSIX behavior, set ENOTDIR when any of the folders in the
	 * file path is a regular file, otherwise set ENOENT.
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


/*
 * Parts of the The p_readlink function are heavily inspired by the php 
 * readlink function in link_win32.c
 *
 * Copyright (c) 1999 - 2012 The PHP Group. All rights reserved.
 *
 * For details of the PHP license see http://www.php.net/license/3_01.txt
 */
int p_readlink(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPWSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;
	git_win32_path link_w;
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

	git_win32_path_from_c(link_w, link);

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
	git_win32_path buf;
	mode_t mode = 0;

	git_win32_path_from_c(buf, path);

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
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
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
	git_win32_path_as_utf8 target;
	int error = 0;

	error = do_lstat(path, buf, 0);

	/* We need not do this in a loop to unwind chains of symlinks since
	 * p_readlink calls GetFinalPathNameByHandle which does it for us. */
	if (error >= 0 && S_ISLNK(buf->st_mode) &&
		(error = p_readlink(path, target, sizeof(target))) >= 0)
		error = do_lstat(target, buf, 0);

	return error;
}

int p_chdir(const char* path)
{
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
	return _wchdir(buf);
}

int p_chmod(const char* path, mode_t mode)
{
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
	return _wchmod(buf, mode);
}

int p_rmdir(const char* path)
{
	int error;
	git_win32_path buf;
	git_win32_path_from_c(buf, path);

	error = _wrmdir(buf);

	/* _wrmdir() is documented to return EACCES if "A program has an open
	 * handle to the directory."  This sounds like what everybody else calls
	 * EBUSY.  Let's convert appropriate error codes.
	 */
	if (GetLastError() == ERROR_SHARING_VIOLATION)
		errno = EBUSY;

	return error;
}

int p_hide_directory__w32(const char *path)
{
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
	return (SetFileAttributesW(buf, FILE_ATTRIBUTE_HIDDEN) != 0) ? 0 : -1;
}

char *p_realpath(const char *orig_path, char *buffer)
{
	int ret;
	git_win32_path orig_path_w;
	git_win32_path buffer_w;

	git_win32_path_from_c(orig_path_w, orig_path);

	/* Implicitly use GetCurrentDirectory which can be a threading issue */
	ret = GetFullPathNameW(orig_path_w, GIT_WIN_PATH_UTF16, buffer_w, NULL);

	/* According to MSDN, a return value equals to zero means a failure. */
	if (ret == 0 || ret > GIT_WIN_PATH_UTF16)
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
	git_win32_path buf;
	git_win32_path_from_c(buf, path);
	return _waccess(buf, mode);
}

int p_rename(const char *from, const char *to)
{
	git_win32_path wfrom;
	git_win32_path wto;
	int rename_tries;
	int rename_succeeded;
	int error;

	git_win32_path_from_c(wfrom, from);
	git_win32_path_from_c(wto, to);
	
	/* wait up to 50ms if file is locked by another thread or process */
	rename_tries = 0;
	rename_succeeded = 0;
	while (rename_tries < 10) {
		if (MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != 0) {
			rename_succeeded = 1;
			break;
		}
		
		error = GetLastError();
		if (error == ERROR_SHARING_VIOLATION || error == ERROR_ACCESS_DENIED) {
			Sleep(5);
			rename_tries++;
		} else
			break;
	}
	
	return rename_succeeded ? 0 : -1;
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

int p_inet_pton(int af, const char *src, void *dst)
{
	struct sockaddr_storage sin;
	void *addr;
	int sin_len = sizeof(struct sockaddr_storage), addr_len;
	int error = 0;

	if (af == AF_INET) {
		addr = &((struct sockaddr_in *)&sin)->sin_addr;
		addr_len = sizeof(struct in_addr);
	} else if (af == AF_INET6) {
		addr = &((struct sockaddr_in6 *)&sin)->sin6_addr;
		addr_len = sizeof(struct in6_addr);
	} else {
		errno = EAFNOSUPPORT;
		return -1;
	}

	if ((error = WSAStringToAddressA((LPSTR)src, af, NULL, (LPSOCKADDR)&sin, &sin_len)) == 0) {
		memcpy(dst, addr, addr_len);
		return 1;
	}

	switch(WSAGetLastError()) {
	case WSAEINVAL:
		return 0;
	case WSAEFAULT:
		errno = ENOSPC;
		return -1;
	case WSA_NOT_ENOUGH_MEMORY:
		errno = ENOMEM;
		return -1;
	}

	errno = EINVAL;
	return -1;
}
