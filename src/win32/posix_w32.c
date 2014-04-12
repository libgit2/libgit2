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
#include "global.h"
#include "reparse.h"
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <ws2tcpip.h>

static HINSTANCE win32_kernel32_dll;

typedef DWORD (WINAPI *win32_getfinalpath_fn)(HANDLE, LPWSTR, DWORD, DWORD);

static win32_getfinalpath_fn win32_getfinalpath;

static void win32_posix_shutdown(void)
{
	if (win32_kernel32_dll) {
		FreeLibrary(win32_kernel32_dll);
		win32_kernel32_dll = NULL;
	}
}

static int win32_posix_initialize(void)
{
	if (win32_kernel32_dll)
		return 0;

	win32_kernel32_dll = LoadLibrary("kernel32.dll");

	if (!win32_kernel32_dll) {
		giterr_set(GITERR_OS, "Could not load 'kernel32.dll'");
		return -1;
	}

	win32_getfinalpath = (win32_getfinalpath_fn)
		GetProcAddress(win32_kernel32_dll, "GetFinalPathNameByHandleW");

	if (!win32_getfinalpath) {
		giterr_set(GITERR_OS, "Could not load 'GetFinalPathNameByHandleW'");
		return -1;
	}

	git__on_shutdown(win32_posix_shutdown);

	return 0;
}


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

static int readlink_w(const git_win32_path path, wchar_t *out, size_t out_len)
{
	unsigned char buf[MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	GIT_REPARSE_DATA_BUFFER *reparse_buf = (GIT_REPARSE_DATA_BUFFER *)buf;
	HANDLE handle;
	DWORD ioctl_ret;
	wchar_t *target;
	int target_len;
	int error = 0;

	handle = CreateFileW(path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		errno = ENOENT;
		return -1;
	}

	if (!DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0,
		reparse_buf, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &ioctl_ret, NULL)) {
		errno = EINVAL;
		error = -1;
		goto on_error;
	}

	/* Not all reparse points are links to other files; only symlinks and
	 * junctions ("mount points") should be resolved as links.
	 */

	switch(reparse_buf->ReparseTag) {
	case IO_REPARSE_TAG_SYMLINK:
		target = reparse_buf->SymbolicLinkReparseBuffer.PathBuffer +
			(reparse_buf->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR));
		target_len = reparse_buf->SymbolicLinkReparseBuffer.SubstituteNameLength;

		break;
	case IO_REPARSE_TAG_MOUNT_POINT:
		target = reparse_buf->MountPointReparseBuffer.PathBuffer +
			(reparse_buf->MountPointReparseBuffer.SubstituteNameOffset / sizeof(WCHAR));
		target_len = reparse_buf->MountPointReparseBuffer.SubstituteNameLength;

		break;
	default:
		errno = EINVAL;
		error = -1;
		goto on_error;
	}

	if (target_len > 0) {
		target_len = git_win32_path_unparse(target, target_len / sizeof(WCHAR));

		if ((target_len = min((int)out_len, target_len)) > 0)
			memcpy(out, target, target_len * sizeof(WCHAR));
	}

	CloseHandle(handle);
	return target_len;

on_error:
	CloseHandle(handle);
	return error;
}

#define WIN32_IS_WSEP(CH) ((CH) == L'/' || (CH) == L'\\')

static int lstat_w(
	wchar_t *path,
	struct stat *buf,
	int posix_enotdir)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;

	if (GetFileAttributesExW(path, GetFileExInfoStandard, &fdata)) {
		int fMode = S_IREAD;

		if (!buf)
			return 0;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			fMode |= S_IFDIR;
		else
			fMode |= S_IFREG;

		if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
			fMode |= S_IWRITE;

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

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
			git_win32_path target;
			int target_len;

			if ((target_len = readlink_w(path, target, sizeof(target)-1)) >= 0) {
				buf->st_mode = (buf->st_mode & ~S_IFMT) | S_IFLNK;
				buf->st_size = target_len;
			}
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
			int path_len = wcslen(path);

			/* remove last directory component */
			for (path_len--; path_len > 0 && !WIN32_IS_WSEP(path[path_len]); path_len--);

			if (path_len <= 0)
				break;

			path[path_len] = L'\0';

			if (GetFileAttributesExW(path, GetFileExInfoStandard, &fdata)) {
				if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					errno = ENOTDIR;
				break;
			}
		}
	}

	return -1;
}

int do_lstat(const char *path, struct stat *buf, bool posixly_correct)
{
	git_win32_path path_w;
	int path_w_len;

	if ((path_w_len = git_win32_path_from_c(path_w, path)) < 0)
		return path_w_len;

	git_win32_path_trim_end(path_w, path_w_len);

	return lstat_w(path_w, buf, posixly_correct);
}

int p_lstat(const char *path, struct stat *buf)
{
	return do_lstat(path, buf, 0);
}

int p_lstat_posixly(const char *path, struct stat *buf)
{
	return do_lstat(path, buf, 1);
}

int p_readlink(const char *link, char *target, size_t target_len)
{
	git_win32_path link_utf16, target_utf16;
	int len;

	if (git__utf8_to_16(link_utf16, sizeof(link_utf16)-1, link) < 0) {
		errno = EINVAL;
		return -1;
	}

	if ((len = readlink_w(link_utf16, target_utf16, sizeof(target_utf16)-1)) < 0)
		return len;

	target_utf16[len] = L'\0';

	return git__utf16_to_8(target, target_len, target_utf16);
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


wchar_t *realpath_w(
	wchar_t *path,
	wchar_t *resolved)
{
	HANDLE handle;
	int len;
	wchar_t *out = NULL;

	if (win32_posix_initialize() < 0)
		return NULL;

	handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		giterr_set(GITERR_OS, "cannot open '%s'", path);
		return NULL;
	}

	if ((len = win32_getfinalpath(handle, resolved, MAX_PATH, 0)) < 0) {
		giterr_set(GITERR_OS, "cannot open '%s'", path);
		goto done;
	}

	if (len > MAX_PATH) {
		errno = ENAMETOOLONG;
		goto done;
	}

	git_win32_path_unparse(resolved, len);

	out = resolved;

done:
	CloseHandle(handle);
	return out;
}

int p_stat(const char *path, struct stat *buf)
{
	git_win32_path path_w, target_w;
	int path_w_len;
	int error = 0;

	if ((path_w_len = git_win32_path_from_c(path_w, path)) < 0)
		return path_w_len;

	git_win32_path_trim_end(path_w, path_w_len);

	if ((error = lstat_w(path_w, buf, 0)) < 0 ||
		!S_ISLNK(buf->st_mode))
		return error;

	if (realpath_w(path_w, target_w) == NULL)
		return -1;

	return lstat_w(target_w, buf, 0);
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
