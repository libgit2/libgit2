/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "findfile.h"

#include "path_w32.h"
#include "utf-conv.h"
#include "fs_path.h"

#define REG_MSYSGIT_INSTALL_LOCAL L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"

#ifndef _WIN64
#define REG_MSYSGIT_INSTALL REG_MSYSGIT_INSTALL_LOCAL
#else
#define REG_MSYSGIT_INSTALL L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"
#endif

static int git_win32__expand_path(git_win32_path dest, const wchar_t *src)
{
	DWORD len = ExpandEnvironmentStringsW(src, dest, GIT_WIN_PATH_UTF16);

	if (!len || len > GIT_WIN_PATH_UTF16)
		return -1;

	return 0;
}

static int win32_path_to_8(git_str *dest, const wchar_t *src)
{
	git_win32_utf8_path utf8_path;

	if (git_win32_path_to_utf8(utf8_path, src) < 0) {
		git_error_set(GIT_ERROR_OS, "unable to convert path to UTF-8");
		return -1;
	}

	/* Convert backslashes to forward slashes */
	git_fs_path_mkposix(utf8_path);

	return git_str_sets(dest, utf8_path);
}

static wchar_t *win32_walkpath(wchar_t *path, wchar_t *buf, size_t buflen)
{
	wchar_t term, *base = path;

	GIT_ASSERT_ARG_WITH_RETVAL(path, NULL);
	GIT_ASSERT_ARG_WITH_RETVAL(buf, NULL);
	GIT_ASSERT_ARG_WITH_RETVAL(buflen, NULL);

	term = (*path == L'"') ? *path++ : L';';

	for (buflen--; *path && *path != term && buflen; buflen--)
		*buf++ = *path++;

	*buf = L'\0'; /* reserved a byte via initial subtract */

	while (*path == term || *path == L';')
		path++;

	return (path != base) ? path : NULL;
}

static int win32_find_git_for_windows_architecture_root(git_win32_path root_path, const wchar_t *subdir)
{
	/* Git for Windows >= 2 comes with a special architecture root (mingw64 and mingw32)
	 * under which the "share" folder is located, check which we need (none is also ok) */

	static const wchar_t *architecture_roots[4] = {
		L"", // starting with Git 2.24 the etc folder is directly in the root folder
		L"mingw64\\",
		L"mingw32\\",
		NULL,
	};

	const wchar_t **roots = architecture_roots;
	size_t root_path_len = wcslen(root_path);

	for (; *roots != NULL; ++roots) {
		git_win32_path tmp_root;
		DWORD subdir_len;
		if (wcscpy(tmp_root, root_path) &&
			root_path_len + wcslen(*roots) <= MAX_PATH &&
			wcscat(tmp_root, *roots) &&
			!_waccess(tmp_root, F_OK)) {
			wcscpy(root_path, tmp_root);
			root_path_len += (DWORD)wcslen(*roots);

			subdir_len = (DWORD)wcslen(subdir);
			if (root_path_len + subdir_len >= MAX_PATH)
				break;

			// append subdir and check whether it exists for the Git installation
			wcscat(tmp_root, subdir);
			if (!_waccess(tmp_root, F_OK)) {
				wcscpy(root_path, tmp_root);
				root_path_len += subdir_len;
				break;
			}
		}
	}

	return 0;
}

static int win32_find_git_in_path(git_str *buf, const wchar_t *gitexe, const wchar_t *subdir)
{
	wchar_t *path, *env, lastch;
	git_win32_path root;
	size_t gitexe_len = wcslen(gitexe);
	DWORD len;
	bool found = false;

	len = GetEnvironmentVariableW(L"PATH", NULL, 0);

	if (len < 0)
	    return -1;

	path = git__malloc(len * sizeof(wchar_t));
	GIT_ERROR_CHECK_ALLOC(path);

	len = GetEnvironmentVariableW(L"PATH", path, len);

	if (len < 0)
	    return -1;

	env = path;

	while ((env = win32_walkpath(env, root, MAX_PATH-1)) && *root) {
		size_t root_len = wcslen(root);
		lastch = root[root_len - 1];

		/* ensure trailing slash (MAX_PATH-1 to walkpath guarantees space) */
		if (lastch != L'/' && lastch != L'\\') {
			root[root_len++] = L'\\';
			root[root_len]   = L'\0';
		}

		if (root_len + gitexe_len >= MAX_PATH)
			continue;

		if (!_waccess(root, F_OK)) {
			/* check whether we found a Git for Windows installation and do some path adjustments OR just append subdir */
			if ((root_len > 5 && wcscmp(root - 4, L"cmd\\")) || wcscmp(root - 4, L"bin\\")) {
				/* strip "bin" or "cmd" and try to find architecture root for appending subdir */
				root_len -= 4;
				root[root_len] = L'\0';
				if (win32_find_git_for_windows_architecture_root(root, subdir))
					continue;
			} else {
				if (root_len + wcslen(subdir) >= MAX_PATH)
					continue;
				wcscat(root, subdir);
			}

			win32_path_to_8(buf, root);
			found = true;
			break;
		}
	}

	git__free(path);
	return found ? 0 : GIT_ENOTFOUND;
}

static int win32_find_git_in_registry(
	git_str *buf, const HKEY hive, const wchar_t *key, const wchar_t *subdir)
{
	HKEY hKey;
	int error = GIT_ENOTFOUND;

	GIT_ASSERT_ARG(buf);

	if (!RegOpenKeyExW(hive, key, 0, KEY_READ, &hKey)) {
		DWORD dwType, cbData;
		git_win32_path path;

		/* Ensure that the buffer is big enough to have the suffix attached
		 * after we receive the result. */
		cbData = (DWORD)(sizeof(path) - wcslen(subdir) * sizeof(wchar_t));

		/* InstallLocation points to the root of the git directory */
		if (!RegQueryValueExW(hKey, L"InstallLocation", NULL, &dwType, (LPBYTE)path, &cbData) &&
			dwType == REG_SZ) {

			/* Convert to UTF-8, with forward slashes, and output the path
			 * to the provided buffer */
			if (!win32_find_git_for_windows_architecture_root(path, subdir) &&
				!win32_path_to_8(buf, path))
				error = 0;
		}

		RegCloseKey(hKey);
	}

	return error;
}

static int win32_find_existing_dirs(
	git_str *out, const wchar_t *tmpl[])
{
	git_win32_path path16;
	git_str buf = GIT_STR_INIT;

	git_str_clear(out);

	for (; *tmpl != NULL; tmpl++) {
		if (!git_win32__expand_path(path16, *tmpl) &&
			path16[0] != L'%' &&
			!_waccess(path16, F_OK))
		{
			win32_path_to_8(&buf, path16);

			if (buf.size)
				git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);
		}
	}

	git_str_dispose(&buf);

	return (git_str_oom(out) ? -1 : 0);
}

int git_win32__find_system_dir_in_path(git_str *out, const wchar_t *subdir)
{
    /* directories where git.exe & git.cmd are found */
    if (win32_find_git_in_path(out, L"git.exe", subdir) == 0)
	return 0;

    return win32_find_git_in_path(out, L"git.cmd", subdir);
}

static int git_win32__find_system_dir_in_registry(git_str *out, const wchar_t *subdir)
{
    git_str buf = GIT_STR_INIT;

    /* directories where git is installed according to registry */
    if (!win32_find_git_in_registry(
	&buf, HKEY_CURRENT_USER, REG_MSYSGIT_INSTALL_LOCAL, subdir) && buf.size)
	git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

#ifdef GIT_ARCH_64
    if (!win32_find_git_in_registry(
	&buf, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL_LOCAL, subdir) && buf.size)
	git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);
#endif

    if (!win32_find_git_in_registry(
	&buf, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL, subdir) && buf.size)
	git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

    git_str_dispose(&buf);

    return (git_str_oom(out) ? -1 : 0);
}

int git_win32__find_system_dirs(git_str *out, const wchar_t *subdir)
{
	int error;

	if ((error = git_win32__find_system_dir_in_path(out, subdir)) == 0)
	    error = git_win32__find_system_dir_in_registry(out, subdir);

	return error;
}

int git_win32__find_global_dirs(git_str *out)
{
	static const wchar_t *global_tmpls[4] = {
		L"%HOME%\\",
		L"%HOMEDRIVE%%HOMEPATH%\\",
		L"%USERPROFILE%\\",
		NULL,
	};

	return win32_find_existing_dirs(out, global_tmpls);
}

int git_win32__find_xdg_dirs(git_str *out)
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

int git_win32__find_programdata_dirs(git_str *out)
{
	static const wchar_t *programdata_tmpls[2] = {
		L"%PROGRAMDATA%\\Git",
		NULL,
	};

	return win32_find_existing_dirs(out, programdata_tmpls);
}
