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

#define REG_GITFORWINDOWS_KEY       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"
#define REG_GITFORWINDOWS_KEY_WOW64 L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"

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

static git_win32_path mock_registry;
static bool mock_registry_set;

extern int git_win32__set_registry_system_dir(const wchar_t *mock_sysdir)
{
	if (!mock_sysdir) {
		mock_registry[0] = L'\0';
		mock_registry_set = false;
	} else {
		size_t len = wcslen(mock_sysdir);

		if (len > GIT_WIN_PATH_MAX) {
			git_error_set(GIT_ERROR_INVALID, "mock path too long");
			return -1;
		}

		wcscpy(mock_registry, mock_sysdir);
		mock_registry_set = true;
	}

	return 0;
}

static int lookup_registry_key(
	git_win32_path out,
	const HKEY hive,
	const wchar_t* key,
	const wchar_t *value)
{
	HKEY hkey;
	DWORD type, size;
	int error = GIT_ENOTFOUND;

	/*
	 * Registry data may not be NUL terminated, provide room to do
	 * it ourselves.
	 */
	size = (DWORD)((sizeof(git_win32_path) - 1) * sizeof(wchar_t));

	if (RegOpenKeyExW(hive, key, 0, KEY_READ, &hkey) != 0)
		return GIT_ENOTFOUND;

	if (RegQueryValueExW(hkey, value, NULL, &type, (LPBYTE)out, &size) == 0 &&
	    type == REG_SZ &&
	    size > 0 &&
	    size < sizeof(git_win32_path)) {
		size_t wsize = size / sizeof(wchar_t);
		size_t len = wsize - 1;

		if (out[wsize - 1] != L'\0') {
			len = wsize;
			out[wsize] = L'\0';
		}

		if (out[len - 1] == L'\\')
			out[len - 1] = L'\0';

		if (_waccess(out, F_OK) == 0)
			error = 0;
	}

	RegCloseKey(hkey);
	return error;
}

static int find_sysdir_in_registry(git_win32_path out)
{
	if (mock_registry_set) {
		if (mock_registry[0] == L'\0')
			return GIT_ENOTFOUND;

		wcscpy(out, mock_registry);
		return 0;
	}

	if (lookup_registry_key(out, HKEY_CURRENT_USER, REG_GITFORWINDOWS_KEY, L"InstallLocation") == 0 ||
	    lookup_registry_key(out, HKEY_CURRENT_USER, REG_GITFORWINDOWS_KEY_WOW64, L"InstallLocation") == 0 ||
	    lookup_registry_key(out, HKEY_LOCAL_MACHINE, REG_GITFORWINDOWS_KEY, L"InstallLocation") == 0 ||
	    lookup_registry_key(out, HKEY_LOCAL_MACHINE, REG_GITFORWINDOWS_KEY_WOW64, L"InstallLocation") == 0)
		return 0;

    return GIT_ENOTFOUND;
}

static int find_sysdir_in_path(git_win32_path out)
{
	size_t out_len;

	if (git_win32_path_find_executable(out, L"git.exe") < 0 &&
	    git_win32_path_find_executable(out, L"git.cmd") < 0)
		return GIT_ENOTFOUND;

	out_len = wcslen(out);

	/* Trim the file name */
	if (out_len <= CONST_STRLEN(L"git.exe"))
		return GIT_ENOTFOUND;

	out_len -= CONST_STRLEN(L"git.exe");

	if (out_len && out[out_len - 1] == L'\\')
		out_len--;

	/*
	 * Git for Windows usually places the command in a 'bin' or
	 * 'cmd' directory, trim that.
	 */
	if (out_len >= CONST_STRLEN(L"\\bin") &&
	    wcsncmp(&out[out_len - CONST_STRLEN(L"\\bin")], L"\\bin", CONST_STRLEN(L"\\bin")) == 0)
		out_len -= CONST_STRLEN(L"\\bin");
	else if (out_len >= CONST_STRLEN(L"\\cmd") &&
	         wcsncmp(&out[out_len - CONST_STRLEN(L"\\cmd")], L"\\cmd", CONST_STRLEN(L"\\cmd")) == 0)
		out_len -= CONST_STRLEN(L"\\cmd");

	if (!out_len)
		return GIT_ENOTFOUND;

	out[out_len] = L'\0';
	return 0;
}

static int win32_find_existing_dirs(
    git_str* out,
    const wchar_t* tmpl[])
{
	git_win32_path path16;
	git_str buf = GIT_STR_INIT;

	git_str_clear(out);

	for (; *tmpl != NULL; tmpl++) {
		if (!git_win32__expand_path(path16, *tmpl) &&
		    path16[0] != L'%' &&
		    !_waccess(path16, F_OK)) {
			win32_path_to_8(&buf, path16);

			if (buf.size)
				git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);
		}
	}

	git_str_dispose(&buf);

	return (git_str_oom(out) ? -1 : 0);
}

static int append_subdir(git_str *out, git_str *path, const char *subdir)
{
	static const char* architecture_roots[] = {
		"",
		"mingw64",
		"mingw32",
		NULL
	};
	const char **root;
	size_t orig_path_len = path->size;

	for (root = architecture_roots; *root; root++) {
		if ((*root[0] && git_str_joinpath(path, path->ptr, *root) < 0) ||
		    git_str_joinpath(path, path->ptr, subdir) < 0)
			return -1;

		if (git_fs_path_exists(path->ptr) &&
		    git_str_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, path->ptr) < 0)
			return -1;

		git_str_truncate(path, orig_path_len);
	}

	return 0;
}

int git_win32__find_system_dirs(git_str *out, const char *subdir)
{
	git_win32_path pathdir, regdir;
	git_str path8 = GIT_STR_INIT;
	bool has_pathdir, has_regdir;
	int error;

	has_pathdir = (find_sysdir_in_path(pathdir) == 0);
	has_regdir = (find_sysdir_in_registry(regdir) == 0);

	if (!has_pathdir && !has_regdir)
		return GIT_ENOTFOUND;

	/*
	 * Usually the git in the path is the same git in the registry,
	 * in this case there's no need to duplicate the paths.
	 */
	if (has_pathdir && has_regdir && wcscmp(pathdir, regdir) == 0)
		has_regdir = false;

	if (has_pathdir) {
		if ((error = win32_path_to_8(&path8, pathdir)) < 0 ||
		    (error = append_subdir(out, &path8, subdir)) < 0)
			goto done;
	}

	if (has_regdir) {
		if ((error = win32_path_to_8(&path8, regdir)) < 0 ||
		    (error = append_subdir(out, &path8, subdir)) < 0)
			goto done;
	}

done:
    git_str_dispose(&path8);
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
