/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "utf-conv.h"
#include "path.h"
#include "findfile.h"

#define REG_MSYSGIT_INSTALL_LOCAL L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"

#ifndef _WIN64
#define REG_MSYSGIT_INSTALL REG_MSYSGIT_INSTALL_LOCAL
#else
#define REG_MSYSGIT_INSTALL L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"
#endif

int git_win32__expand_path(struct git_win32__path *s_root, const wchar_t *templ)
{
	s_root->len = ExpandEnvironmentStringsW(templ, s_root->path, MAX_PATH);
	return s_root->len ? 0 : -1;
}

static int win32_path_to_8(git_buf *path_utf8, const wchar_t *path)
{
	char temp_utf8[GIT_PATH_MAX];

	git__utf16_to_8(temp_utf8, GIT_PATH_MAX, path);
	git_path_mkposix(temp_utf8);

	return git_buf_sets(path_utf8, temp_utf8);
}

int git_win32__find_file(
	git_buf *path, const struct git_win32__path *root, const char *filename)
{
	size_t len, alloc_len;
	wchar_t *file_utf16 = NULL;

	if (!root || !filename || (len = strlen(filename)) == 0)
		return GIT_ENOTFOUND;

	/* allocate space for wchar_t path to file */
	alloc_len = root->len + len + 2;
	file_utf16 = git__calloc(alloc_len, sizeof(wchar_t));
	GITERR_CHECK_ALLOC(file_utf16);

	/* append root + '\\' + filename as wchar_t */
	memcpy(file_utf16, root->path, root->len * sizeof(wchar_t));

	if (*filename == '/' || *filename == '\\')
		filename++;

	git__utf8_to_16(file_utf16 + root->len - 1, alloc_len - root->len, filename);

	/* check access */
	if (_waccess(file_utf16, F_OK) < 0) {
		git__free(file_utf16);
		return GIT_ENOTFOUND;
	}

	win32_path_to_8(path, file_utf16);
	git__free(file_utf16);

	return 0;
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

static int win32_find_git_in_path(git_buf *buf, const wchar_t *gitexe, const wchar_t *subdir)
{
	wchar_t *env = _wgetenv(L"PATH"), lastch;
	struct git_win32__path root;
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

		if (_waccess(root.path, F_OK) == 0 && root.len > 5) {
			/* replace "bin\\" or "cmd\\" with subdir */
			wcscpy(&root.path[root.len - 4], subdir);

			win32_path_to_8(buf, root.path);
			return 0;
		}
	}

	return GIT_ENOTFOUND;
}

static int win32_find_git_in_registry(
	git_buf *buf, const HKEY hieve, const wchar_t *key, const wchar_t *subdir)
{
	HKEY hKey;
	DWORD dwType = REG_SZ;
	struct git_win32__path path16;

	assert(buf);

	path16.len = MAX_PATH;

	if (RegOpenKeyExW(hieve, key, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
		if (RegQueryValueExW(hKey, L"InstallLocation", NULL, &dwType,
			(LPBYTE)&path16.path, &path16.len) == ERROR_SUCCESS)
		{
			/* InstallLocation points to the root of the git directory */

			if (path16.len + 4 > MAX_PATH) { /* 4 = wcslen(L"etc\\") */
				giterr_set(GITERR_OS, "Cannot locate git - path too long");
				return -1;
			}

			wcscat(path16.path, subdir);
			path16.len += 4;

			win32_path_to_8(buf, path16.path);
		}

		RegCloseKey(hKey);
	}

	return path16.len ? 0 : GIT_ENOTFOUND;
}

static int win32_find_existing_dirs(
	git_buf *out, const wchar_t *tmpl[])
{
	struct git_win32__path path16;
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

	git_buf_free(&buf);

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

	if (!win32_find_git_in_registry(
			&buf, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL, subdir) && buf.size)
		git_buf_join(out, GIT_PATH_LIST_SEPARATOR, out->ptr, buf.ptr);

	git_buf_free(&buf);

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
