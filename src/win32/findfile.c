/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "findfile.h"

#include "path_w32.h"
#include "utf-conv.h"
#include "path.h"

#define REG_MSYSGIT_INSTALL_LOCAL L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"

#ifndef _WIN64
#define REG_MSYSGIT_INSTALL REG_MSYSGIT_INSTALL_LOCAL
#else
#define REG_MSYSGIT_INSTALL L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"
#endif

typedef struct {
	git_win32_path path;
	DWORD len;
} _findfile_path;

static int git_win32__expand_path(_findfile_path *dest, const wchar_t *src)
{
	dest->len = ExpandEnvironmentStringsW(src, dest->path, ARRAY_SIZE(dest->path));

	if (!dest->len || dest->len > ARRAY_SIZE(dest->path))
		return -1;

	return 0;
}

static int win32_path_to_8(git_buf *dest, const wchar_t *src)
{
	git_win32_utf8_path utf8_path;

	if (git_win32_path_to_utf8(utf8_path, src) < 0) {
		git_error_set(GIT_ERROR_OS, "unable to convert path to UTF-8");
		return -1;
	}

	/* Convert backslashes to forward slashes */
	git_path_mkposix(utf8_path);

	return git_buf_sets(dest, utf8_path);
}

static wchar_t* win32_walkpath(wchar_t *path, wchar_t *buf, size_t buflen)
{
	wchar_t term, *base = path;

	assert(path && buf && buflen);

	term = (*path == L'"') ? *path++ : L';';

	for (buflen--; *path && *path != term && buflen; buflen--)
		*buf++ = *path++;

	*buf = L'\0'; /* reserved a byte via initial subtract */

	while (*path == term || *path == L';')
		path++;

	return (path != base) ? path : NULL;
}

static int win32_find_git_for_windows_architecture_root(_findfile_path *root_path, const wchar_t *subdir)
{
	/* Git for Windows >= 2 comes with a special architecture root (mingw64 and mingw32)
	 * under which "etc" and the "share" folders are located, check which we need (none is also ok) */

	static const wchar_t *architecture_roots[3] = {
		L"mingw64\\",
		L"mingw32\\",
		NULL,
	};

	const wchar_t **roots = architecture_roots;
	DWORD subdir_len;
	git_win32_path tmp_root;
	for (; *roots != NULL; ++roots) {
		if (wcscpy(tmp_root, root_path->path) &&
			root_path->len + wcslen(*roots) <= MAX_PATH &&
			wcscat(tmp_root, *roots) &&
			!_waccess(tmp_root, F_OK))
		{
			wcscpy(root_path->path, tmp_root);
			root_path->len += wcslen(*roots);
			break;
		}
	}

	subdir_len = (DWORD)wcslen(subdir);
	if (root_path->len + subdir_len >= MAX_PATH)
		return -1;

	wcscat(root_path->path, subdir);
	root_path->len += subdir_len;

	return 0;
}

static int win32_find_git_in_path(git_buf *buf, const wchar_t *gitexe, const wchar_t *subdir)
{
	wchar_t *env = _wgetenv(L"PATH"), lastch;
	_findfile_path root;
	size_t gitexe_len = wcslen(gitexe);

	if (!env)
		return -1;

	while ((env = win32_walkpath(env, root.path, MAX_PATH-1)) && *root.path) {
		root.len = (DWORD)wcslen(root.path);
		lastch = root.path[root.len - 1];

		/* ensure trailing slash (MAX_PATH-1 to walkpath guarantees space) */
		if (lastch != L'/' && lastch != L'\\') {
			root.path[root.len++] = L'\\';
			root.path[root.len]   = L'\0';
		}

		if (root.len + gitexe_len >= MAX_PATH)
			continue;
		wcscpy(&root.path[root.len], gitexe);

		if (!_waccess(root.path, F_OK)) {
			/* check whether we found a Git for Windows installation and do some path adjustments OR just append subdir */
			if (root.len > 5 && wcscmp(root.path - 4, L"cmd\\") || wcscmp(root.path - 4, L"bin\\")) {
				/* strip "bin" or "cmd" and try to find architecture root for appending subdir */
				root.len -= 4;
				root.path[root.len] = L'\0';
				if (win32_find_git_for_windows_architecture_root(&root, subdir))
					continue;
			} else {
				if (root.len + wcslen(subdir) >= MAX_PATH)
					continue;
				wcscat(root.path, subdir);
			}

			win32_path_to_8(buf, root.path);
			return 0;
		}
	}

	return GIT_ENOTFOUND;
}

static int win32_find_git_in_registry(
	git_buf *buf, const HKEY hive, const wchar_t *key, const wchar_t *subdir)
{
	HKEY hKey;
	int error = GIT_ENOTFOUND;

	assert(buf);

	if (!RegOpenKeyExW(hive, key, 0, KEY_READ, &hKey)) {
		DWORD dwType, cbData;
		_findfile_path path;

		/* Ensure that the buffer is big enough to have the suffix attached
		 * after we receive the result. */
		cbData = (DWORD)(sizeof(path) - wcslen(subdir) * sizeof(wchar_t));

		/* InstallLocation points to the root of the git directory */
		if (!RegQueryValueExW(hKey, L"InstallLocation", NULL, &dwType, (LPBYTE)path.path, &cbData) &&
			dwType == REG_SZ) {
			path.len = cbData;

			/* Convert to UTF-8, with forward slashes, and output the path
			 * to the provided buffer */
			if (!win32_find_git_for_windows_architecture_root(&path, subdir) &&
				!win32_path_to_8(buf, path.path))
				error = 0;
		}

		RegCloseKey(hKey);
	}

	return error;
}

static int win32_find_existing_dirs(
	git_buf *out, const wchar_t *tmpl[])
{
	_findfile_path path16;
	git_buf buf = GIT_BUF_INIT;

	git_buf_clear(out);

	for (; *tmpl != NULL; tmpl++) {
		if (!git_win32__expand_path(&path16, *tmpl) &&
			path16.path[0] != L'%' &&
			!_waccess(path16.path, F_OK))
		{
			win32_path_to_8(&buf, path16.path);

			if (buf.size)
				git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);
		}
	}

	git_buf_dispose(&buf);

	return (git_buf_oom(out) ? -1 : 0);
}

int git_win32__find_system_dirs(git_buf *out, const wchar_t *subdir)
{
	git_buf buf = GIT_BUF_INIT;

	/* directories where git.exe & git.cmd are found */
	if (!win32_find_git_in_path(&buf, L"git.exe", subdir) && buf.size)
		git_buf_set(out, buf.ptr, buf.size);
	else
		git_buf_clear(out);

	if (!win32_find_git_in_path(&buf, L"git.cmd", subdir) && buf.size)
		git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

	/* directories where git is installed according to registry */
	if (!win32_find_git_in_registry(
			&buf, HKEY_CURRENT_USER, REG_MSYSGIT_INSTALL_LOCAL, subdir) && buf.size)
		git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

#ifdef WIN64
	if (!win32_find_git_in_registry(
			&buf, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL_LOCAL, subdir) && buf.size)
		git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);
#endif

	if (!win32_find_git_in_registry(
			&buf, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL, subdir) && buf.size)
		git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

	git_buf_dispose(&buf);

	return (git_buf_oom(out) ? -1 : 0);
}

int git_win32__find_global_dirs(git_buf *out)
{
	static const wchar_t *global_tmpls[4] = {
		L"%HOME%\\",
		L"%HOMEDRIVE%%HOMEPATH%\\",
		L"%USERPROFILE%\\",
		NULL,
	};

	return win32_find_existing_dirs(out, global_tmpls);
}

int git_win32__find_xdg_dirs(git_buf *out)
{
	static const wchar_t *global_tmpls[7] = {
		L"%XDG_CONFIG_HOME%\\git",
		L"%APPDATA%\\git",
		L"%LOCALAPPDATA%\\git",
		L"%HOME%\\.config\\git",
		L"%HOMEDRIVE%%HOMEPATH%\\.config\\git",
		L"%USERPROFILE%\\.config\\git",
		NULL,
	};

	return win32_find_existing_dirs(out, global_tmpls);
}

int git_win32__find_programdata_dirs(git_buf *out)
{
	static const wchar_t *programdata_tmpls[2] = {
		L"%PROGRAMDATA%\\Git",
		NULL,
	};

	return win32_find_existing_dirs(out, programdata_tmpls);
}
