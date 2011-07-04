#include "posix.h"
#include "path.h"
#include <errno.h>

int p_unlink(const char *path)
{
	chmod(path, 0666);
	return unlink(path);
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

	if (GetFileAttributesExA(file_name, GetFileExInfoStandard, &fdata)) {
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
		buf->st_size = fdata.nFileSizeLow; /* Can't use nFileSizeHigh, since it's not a stat64 */
		buf->st_dev = buf->st_rdev = (_getdrive() - 1);
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));
		return GIT_SUCCESS;
	}

	switch (GetLastError()) {
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
	case ERROR_SHARING_BUFFER_EXCEEDED:
		return GIT_EOSERR;

	case ERROR_BUFFER_OVERFLOW:
	case ERROR_NOT_ENOUGH_MEMORY:
		return GIT_ENOMEM;

	default:
		return GIT_EINVALIDPATH;
	}
}

int p_lstat(const char *file_name, struct stat *buf)
{
	int namelen, error;
	char alt_name[GIT_PATH_MAX];

	if ((error = do_lstat(file_name, buf)) == GIT_SUCCESS)
		return GIT_SUCCESS;

	/* if file_name ended in a '/', Windows returned ENOENT;
	 * try again without trailing slashes
	 */
	if (error != GIT_EINVALIDPATH)
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	namelen = strlen(file_name);
	if (namelen && file_name[namelen-1] != '/')
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	while (namelen && file_name[namelen-1] == '/')
		--namelen;

	if (!namelen || namelen >= GIT_PATH_MAX)
		return git__throw(GIT_ENOMEM, "Failed to lstat file");

	memcpy(alt_name, file_name, namelen);
	alt_name[namelen] = 0;
	return do_lstat(alt_name, buf);
}

int p_readlink(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPTSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;

	/*
	 * Try to load the pointer to pGetFinalPath dynamically, because
	 * it is not available in platforms older than Vista
	 */
	if (pGetFinalPath == NULL) {
		HINSTANCE library = LoadLibrary("kernel32");

		if (library != NULL)
			pGetFinalPath = (fpath_func)GetProcAddress(library, "GetFinalPathNameByHandleA");

		if (pGetFinalPath == NULL)
			return git__throw(GIT_EOSERR,
				"'GetFinalPathNameByHandleA' is not available in this platform");
	}

	hFile = CreateFile(link,            // file to open
				 GENERIC_READ,          // open for reading
				 FILE_SHARE_READ,       // share for reading
				 NULL,                  // default security
				 OPEN_EXISTING,         // existing file only
				 FILE_FLAG_BACKUP_SEMANTICS, // normal file
				 NULL);                 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
		return GIT_EOSERR;

	dwRet = pGetFinalPath(hFile, target, target_len, 0x0);
	if (dwRet >= target_len)
		return GIT_ENOMEM;

	CloseHandle(hFile);

	if (dwRet > 4) {
		/* Skip first 4 characters if they are "\\?\" */
		if (target[0] == '\\' && target[1] == '\\' && target[2] == '?' && target[3] ==  '\\') {
			char tmp[GIT_PATH_MAX];
			unsigned int offset = 4;
			dwRet -= 4;

			/* \??\UNC\ */
			if (dwRet > 7 && target[4] == 'U' && target[5] == 'N' && target[6] == 'C') {
				offset += 2;
				dwRet -= 2;
				target[offset] = '\\';
			}

			memcpy(tmp, target + offset, dwRet);
			memcpy(target, tmp, dwRet);
		}
	}

	target[dwRet] = '\0';
	return dwRet;
}

int p_hide_directory__w32(const char *path)
{
	int error;

	error = SetFileAttributes(path, FILE_ATTRIBUTE_HIDDEN) != 0 ?
        GIT_SUCCESS : GIT_ERROR; /* MSDN states a "non zero" value indicates a success */

	if (error < GIT_SUCCESS)
		error = git__throw(GIT_EOSERR, "Failed to hide directory '%s'", path);

	return error;
}

char *p_realpath(const char *orig_path, char *buffer)
{
	int ret, alloc = 0;
	
	if (buffer == NULL) {
		buffer = (char *)git__malloc(GIT_PATH_MAX);
		alloc = 1;
	}

	ret = GetFullPathName(orig_path, GIT_PATH_MAX, buffer, NULL);
	if (!ret || ret > GIT_PATH_MAX) {
		if (alloc) free(buffer);
		return NULL;
	}

	git_path_mkposix(buffer);
	return buffer;
}

