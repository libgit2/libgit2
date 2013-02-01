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

int win32_expand_path(struct win32_path *s_root, const wchar_t *templ)
{
	s_root->len = ExpandEnvironmentStringsW(templ, s_root->path, MAX_PATH);
	return s_root->len ? 0 : -1;
}

int win32_find_file(git_buf *path, const struct win32_path *root, const char *filename)
{
	size_t len, alloc_len;
	wchar_t *file_utf16 = NULL;
	char file_utf8[GIT_PATH_MAX];

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

	git__utf8_to_16(file_utf16 + root->len - 1, alloc_len, filename);

	/* check access */
	if (_waccess(file_utf16, F_OK) < 0) {
		git__free(file_utf16);
		return GIT_ENOTFOUND;
	}

	git__utf16_to_8(file_utf8, file_utf16);
	git_path_mkposix(file_utf8);
	git_buf_sets(path, file_utf8);

	git__free(file_utf16);
	return 0;
}

wchar_t* win32_nextpath(wchar_t *path, wchar_t *buf, size_t buflen)
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

int win32_find_system_file_using_path(git_buf *path, const char *filename)
{
	wchar_t * env = NULL;
	struct win32_path root;

	env = _wgetenv(L"PATH");
	if (!env)
		return -1;

	// search in all paths defined in PATH
	while ((env = win32_nextpath(env, root.path, MAX_PATH - 1)) != NULL && *root.path)
	{
		wchar_t * pfin = root.path + wcslen(root.path) - 1; // last char of the current path entry

		// ensure trailing slash
		if (*pfin != L'/' && *pfin != L'\\')
			wcscpy(++pfin, L"\\"); // we have enough space left, MAX_PATH - 1 is used in nextpath above

		root.len = (DWORD)wcslen(root.path) + 1;

		if (win32_find_file(path, &root, "git.cmd") == 0 || win32_find_file(path, &root, "git.exe") == 0) {
			// we found the cmd or bin directory of a git installaton
			if (root.len > 5) {
				wcscpy(root.path + wcslen(root.path) - 4, L"etc\\");
				if (win32_find_file(path, &root, filename) == 0)
					return 0;
			}
		}
	}
	
	return GIT_ENOTFOUND;
}

int win32_find_system_file_using_registry(git_buf *path, const char *filename)
{
	struct win32_path root;

	if (win32_find_msysgit_in_registry(&root, HKEY_CURRENT_USER, REG_MSYSGIT_INSTALL_LOCAL)) {
		if (win32_find_msysgit_in_registry(&root, HKEY_LOCAL_MACHINE, REG_MSYSGIT_INSTALL)) {
			giterr_set(GITERR_OS, "Cannot locate the system's msysgit directory");
			return -1;
		}
	}

	if (win32_find_file(path, &root, filename) < 0) {
		giterr_set(GITERR_OS, "The system file '%s' doesn't exist", filename);
		git_buf_clear(path);
		return GIT_ENOTFOUND;
	}

	return 0;
}

int win32_find_msysgit_in_registry(struct win32_path *root, const HKEY hieve, const wchar_t *key)
{
	HKEY hKey;
	DWORD dwType = REG_SZ;
	DWORD dwSize = MAX_PATH;

	assert(root);

	root->len = 0;
	if (RegOpenKeyExW(hieve, key, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
		if (RegQueryValueExW(hKey, L"InstallLocation", NULL, &dwType, (LPBYTE)&root->path, &dwSize) == ERROR_SUCCESS) {
			// InstallLocation points to the root of the msysgit directory
			if (dwSize + 4 > MAX_PATH) {// 4 = wcslen(L"etc\\")
				giterr_set(GITERR_OS, "Cannot locate the system's msysgit directory - path too long");
				return -1;
			}
			wcscat(root->path, L"etc\\");
			root->len = (DWORD)wcslen(root->path) + 1;
		}
	}
	RegCloseKey(hKey);

	return root->len ? 0 : GIT_ENOTFOUND;
}
