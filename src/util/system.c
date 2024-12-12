/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2_util.h"

#include "system.h"
#include "str.h"
#ifndef GIT_WIN32
# include <unistd.h>
# include <pwd.h>
#endif

#ifndef GIT_WIN32
static int get_passwd_home(git_str *out, uid_t uid)
{
	struct passwd pwd, *pwdptr;
	char *buf = NULL;
	long buflen;
	int error;

	GIT_ASSERT_ARG(out);

	if ((buflen = sysconf(_SC_GETPW_R_SIZE_MAX)) == -1)
		buflen = 1024;

	do {
		buf = git__realloc(buf, buflen);
		error = getpwuid_r(uid, &pwd, buf, buflen, &pwdptr);
		buflen *= 2;
	} while (error == ERANGE && buflen <= 8192);

	if (error) {
		git_error_set(GIT_ERROR_OS, "failed to get passwd entry");
		goto out;
	}

	if (!pwdptr) {
		git_error_set(GIT_ERROR_OS, "no passwd entry found for user");
		goto out;
	}

	if ((error = git_str_puts(out, pwdptr->pw_dir)) < 0)
		goto out;

out:
	git__free(buf);
	return error;
}
#endif

int git_system_homedir(git_str *out)
{
#ifdef GIT_WIN32
	static const wchar_t *global_tmpls[4] = {
		L"%HOME%\\",
		L"%HOMEDRIVE%%HOMEPATH%\\",
		L"%USERPROFILE%\\",
		NULL
	};

	return find_win32_dirs(out, global_tmpls);
#else
	int error;
	uid_t uid, euid;
	const char *sandbox_id;

	uid = getuid();
	euid = geteuid();

	/**
	 * If APP_SANDBOX_CONTAINER_ID is set, we are running in a
	 * sandboxed environment on macOS.
	 */
	sandbox_id = getenv("APP_SANDBOX_CONTAINER_ID");

	/*
	 * In case we are running setuid, use the configuration
	 * of the effective user.
	 *
	 * If we are running in a sandboxed environment on macOS,
	 * we have to get the HOME dir from the password entry file.
	 */
	if (!sandbox_id && uid == euid)
		error = git__getenv(out, "HOME");
	else
		error = get_passwd_home(out, euid);

	return error;
#endif
}
