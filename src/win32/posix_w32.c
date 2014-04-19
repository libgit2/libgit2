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

#if defined(__MINGW32__)
# define FILE_NAME_NORMALIZED 0
#endif

/* GetFinalPathNameByHandleW signature */
typedef DWORD(WINAPI *PFGetFinalPathNameByHandleW)(HANDLE, LPWSTR, DWORD, DWORD);

/* Helper function which converts UTF-8 paths to UTF-16.
 * On failure, errno is set. */
static int utf8_to_16_with_errno(git_win32_path dest, const char *src)
{
	int len = git_win32_path_from_utf8(dest, src);

	if (len < 0) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			errno = ENAMETOOLONG;
		else
			errno = EINVAL; /* Bad code point, presumably */
	}

	return len;
}

int p_mkdir(const char *path, mode_t mode)
{
	git_win32_path buf;

	GIT_UNUSED(mode);

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	return _wmkdir(buf);
}

int p_unlink(const char *path)
{
	git_win32_path buf;
	int error;

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	error = _wunlink(buf);

	/* If the file could not be deleted because it was
	 * read-only, clear the bit and try again */
	if (-1 == error && EACCES == errno) {
		_wchmod(buf, 0666);
		error = _wunlink(buf);
	}

	return error;
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

	if ((flen = utf8_to_16_with_errno(fbuf, file_name)) < 0)
		return -1;

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
			git_win32_utf8_path target;

			if (p_readlink(file_name, target, GIT_WIN_PATH_UTF8) == -1)
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
 * Returns the address of the GetFinalPathNameByHandleW function.
 * This function is available on Windows Vista and higher.
 */
static PFGetFinalPathNameByHandleW get_fpnbyhandle(void)
{
	static PFGetFinalPathNameByHandleW pFunc = NULL;
	PFGetFinalPathNameByHandleW toReturn = pFunc;

	if (!toReturn) {
		HMODULE hModule = GetModuleHandleW(L"kernel32");

		if (hModule)
			toReturn = (PFGetFinalPathNameByHandleW)GetProcAddress(hModule, "GetFinalPathNameByHandleW");

		pFunc = toReturn;
	}

	assert(toReturn);

	return toReturn;
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
	static const wchar_t prefix[] = L"\\\\?\\";
	PFGetFinalPathNameByHandleW pgfp = get_fpnbyhandle();
	HANDLE hFile = NULL;
	wchar_t *target_w = NULL;
	bool trim_prefix;
	git_win32_path link_w;
	DWORD dwChars, dwLastError;
	int error = -1;

	/* Check that we found the function, and convert to UTF-16 */
	if (!pgfp || utf8_to_16_with_errno(link_w, link) < 0)
		goto on_error;

	/* Use FILE_FLAG_BACKUP_SEMANTICS so we can open a directory. Do not
	 * specify FILE_FLAG_OPEN_REPARSE_POINT; we want to open a handle to the
	 * target of the link. */
	hFile = CreateFileW(link_w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		goto on_error;

	/* Find out how large the buffer should be to hold the result */
	if (!(dwChars = pgfp(hFile, NULL, 0, FILE_NAME_NORMALIZED)))
		goto on_error;

	if (!(target_w = git__malloc(dwChars * sizeof(wchar_t))))
		goto on_error;

	/* Call a second time */
	dwChars = pgfp(hFile, target_w, dwChars, FILE_NAME_NORMALIZED);

	if (!dwChars)
		goto on_error;

	/* Do we need to trim off a \\?\ from the start of the path? */
	trim_prefix = (dwChars >= ARRAY_SIZE(prefix)) &&
		!wcsncmp(prefix, target_w, ARRAY_SIZE(prefix));

	/* Convert the result to UTF-8 */
	if (git__utf16_to_8(target, target_len, trim_prefix ? target_w + 4 : target_w) < 0)
		goto on_error;

	error = 0;

on_error:
	dwLastError = GetLastError();

	if (hFile && INVALID_HANDLE_VALUE != hFile)
		CloseHandle(hFile);

	if (target_w)
		git__free(target_w);

	SetLastError(dwLastError);
	return error;
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

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

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

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	return _wopen(buf, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, mode);
}

int p_getcwd(char *buffer_out, size_t size)
{
	git_win32_path buf;
	wchar_t *cwd = _wgetcwd(buf, GIT_WIN_PATH_UTF16);

	if (!cwd)
		return -1;

	/* Convert the working directory back to UTF-8 */
	if (git__utf16_to_8(buffer_out, size, cwd) < 0) {
		DWORD code = GetLastError();

		if (code == ERROR_INSUFFICIENT_BUFFER)
			errno = ERANGE;
		else
			errno = EINVAL;

		return -1;
	}

	return 0;
}

int p_stat(const char* path, struct stat* buf)
{
	git_win32_utf8_path target;
	int error = 0;

	error = do_lstat(path, buf, 0);

	/* We need not do this in a loop to unwind chains of symlinks since
	 * p_readlink calls GetFinalPathNameByHandle which does it for us. */
	if (error >= 0 && S_ISLNK(buf->st_mode) &&
		(error = p_readlink(path, target, GIT_WIN_PATH_UTF8)) >= 0)
		error = do_lstat(target, buf, 0);

	return error;
}

int p_chdir(const char* path)
{
	git_win32_path buf;

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	return _wchdir(buf);
}

int p_chmod(const char* path, mode_t mode)
{
	git_win32_path buf;

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	return _wchmod(buf, mode);
}

int p_rmdir(const char* path)
{
	git_win32_path buf;
	int error;

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	error = _wrmdir(buf);

	if (-1 == error) {
		switch (GetLastError()) {
			/* _wrmdir() is documented to return EACCES if "A program has an open
			 * handle to the directory."  This sounds like what everybody else calls
			 * EBUSY.  Let's convert appropriate error codes.
			 */
			case ERROR_SHARING_VIOLATION:
				errno = EBUSY;
				break;

			/* This error can be returned when trying to rmdir an extant file. */
			case ERROR_DIRECTORY:
				errno = ENOTDIR;
				break;
		}
	}

	return error;
}

char *p_realpath(const char *orig_path, char *buffer)
{
	git_win32_path orig_path_w, buffer_w;

	if (utf8_to_16_with_errno(orig_path_w, orig_path) < 0)
		return NULL;

	/* Note that if the path provided is a relative path, then the current directory
	 * is used to resolve the path -- which is a concurrency issue because the current
	 * directory is a process-wide variable. */
	if (!GetFullPathNameW(orig_path_w, GIT_WIN_PATH_UTF16, buffer_w, NULL)) {
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			errno = ENAMETOOLONG;
		else
			errno = EINVAL;

		return NULL;
	}

	/* The path must exist. */
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributesW(buffer_w)) {
		errno = ENOENT;
		return NULL;
	}

	/* Convert the path to UTF-8. */
	if (buffer) {
		/* If the caller provided a buffer, then it is assumed to be GIT_WIN_PATH_UTF8
		 * characters in size. If it isn't, then we may overflow. */
		if (git__utf16_to_8(buffer, GIT_WIN_PATH_UTF8, buffer_w) < 0)
			return NULL;
	} else {
		/* If the caller did not provide a buffer, then we allocate one for the caller
		 * from the heap. */
		if (git__utf16_to_8_alloc(&buffer, buffer_w) < 0)
			return NULL;
	}

	/* Convert backslashes to forward slashes */
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

int p_access(const char* path, mode_t mode)
{
	git_win32_path buf;

	if (utf8_to_16_with_errno(buf, path) < 0)
		return -1;

	return _waccess(buf, mode);
}

int p_rename(const char *from, const char *to)
{
	git_win32_path wfrom;
	git_win32_path wto;
	int rename_tries;
	int rename_succeeded;
	int error;

	if (utf8_to_16_with_errno(wfrom, from) < 0 ||
		utf8_to_16_with_errno(wto, to) < 0)
		return -1;
	
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
