#ifdef __APPLE__
#include <sys/syslimits.h>
#endif

/*
 * The tempdir is the temporary directory for the entirety of the clar
 * process execution. The sandbox is an individual temporary directory
 * for the execution of an individual test. Sandboxes are deleted
 * entirely after test execution to avoid pollution across tests.
 */
#define CLAR_PATH_MAX 4096
static char _clar_tempdir[CLAR_PATH_MAX];
size_t _clar_tempdir_len;

static char _clar_sandbox[CLAR_PATH_MAX];

static int
is_valid_tmp_path(const char *path)
{
	STAT_T st;

	if (stat(path, &st) != 0)
		return 0;

	if (!S_ISDIR(st.st_mode))
		return 0;

	return (access(path, W_OK) == 0);
}

static int
find_tmp_path(char *buffer, size_t length)
{
#ifndef _WIN32
	static const size_t var_count = 5;
	static const char *env_vars[] = {
		"CLAR_TMP", "TMPDIR", "TMP", "TEMP", "USERPROFILE"
 	};

 	size_t i;

	for (i = 0; i < var_count; ++i) {
		const char *env = getenv(env_vars[i]);
		if (!env)
			continue;

		if (is_valid_tmp_path(env)) {
			if (strlen(env) + 1 > CLAR_PATH_MAX)
				return -1;

			strncpy(buffer, env, length - 1);
			buffer[length - 1] = '\0';
			return 0;
		}
	}

	/* If the environment doesn't say anything, try to use /tmp */
	if (is_valid_tmp_path("/tmp")) {
		strncpy(buffer, "/tmp", length - 1);
		buffer[length - 1] = '\0';
		return 0;
	}

#else
	DWORD env_len = GetEnvironmentVariable("CLAR_TMP", buffer, (DWORD)length);
	if (env_len > 0 && env_len < (DWORD)length)
		return 0;

	if (GetTempPath((DWORD)length, buffer))
		return 0;
#endif

	/* This system doesn't like us, try to use the current directory */
	if (is_valid_tmp_path(".")) {
		strncpy(buffer, ".", length - 1);
		buffer[length - 1] = '\0';
		return 0;
	}

	return -1;
}

static int canonicalize_tmp_path(char *buffer)
{
#ifdef _WIN32
	char tmp[CLAR_PATH_MAX];
	DWORD ret;

	ret = GetFullPathName(buffer, CLAR_PATH_MAX, tmp, NULL);

	if (ret == 0 || ret > CLAR_PATH_MAX)
		return -1;

	ret = GetLongPathName(tmp, buffer, CLAR_PATH_MAX);

	if (ret == 0 || ret > CLAR_PATH_MAX)
		return -1;

	return 0;
#else
	char tmp[CLAR_PATH_MAX];

	if (realpath(buffer, tmp) == NULL)
		return -1;

	strcpy(buffer, tmp);
	return 0;
#endif
}

static void clar_tempdir_shutdown(void)
{
	if (_clar_tempdir[0] == '\0')
		return;

	cl_must_pass(chdir(".."));

	fs_rm(_clar_tempdir);
}

static int build_tempdir_path(void)
{
#ifdef CLAR_TMPDIR
	const char path_tail[] = CLAR_TMPDIR "_XXXXXX";
#else
	const char path_tail[] = "clar_tmp_XXXXXX";
#endif

	size_t len;

	if (find_tmp_path(_clar_tempdir, sizeof(_clar_tempdir)) < 0 ||
	    canonicalize_tmp_path(_clar_tempdir) < 0)
		return -1;

	len = strlen(_clar_tempdir);

#ifdef _WIN32
	{ /* normalize path to POSIX forward slashes */
		size_t i;
		for (i = 0; i < len; ++i) {
			if (_clar_tempdir[i] == '\\')
				_clar_tempdir[i] = '/';
		}
	}
#endif

	if (_clar_tempdir[len - 1] != '/') {
		_clar_tempdir[len++] = '/';
	}

	strncpy(_clar_tempdir + len, path_tail, sizeof(_clar_tempdir) - len);

#if defined(__MINGW32__)
	if (_mktemp(_clar_tempdir) == NULL)
		return -1;

	if (mkdir(_clar_tempdir, 0700) != 0)
		return -1;
#elif defined(_WIN32)
	if (_mktemp_s(_clar_tempdir, sizeof(_clar_tempdir)) != 0)
		return -1;

	if (mkdir(_clar_tempdir, 0700) != 0)
		return -1;
#else
	if (mkdtemp(_clar_tempdir) == NULL)
		return -1;
#endif

	_clar_tempdir_len = strlen(_clar_tempdir);
	return 0;
}

static int clar_tempdir_init(void)
{
	if (_clar_tempdir[0] == '\0' && build_tempdir_path() < 0)
		return -1;

	if (chdir(_clar_tempdir) != 0)
		return -1;

	srand(clock() ^ time(NULL) ^ (getpid() << 16));

	return 0;
}

static int clar_sandbox_create(void)
{
	char alpha[] = "0123456789abcdef";
	int num = rand();

	cl_assert(_clar_sandbox[0] == '\0');

	strcpy(_clar_sandbox, _clar_tempdir);
	_clar_sandbox[_clar_tempdir_len] = '/';

	_clar_sandbox[_clar_tempdir_len + 1] = alpha[(num & 0xf0000000) >> 28];
	_clar_sandbox[_clar_tempdir_len + 2] = alpha[(num & 0x0f000000) >> 24];
	_clar_sandbox[_clar_tempdir_len + 3] = alpha[(num & 0x00f00000) >> 20];
	_clar_sandbox[_clar_tempdir_len + 4] = alpha[(num & 0x000f0000) >> 16];
	_clar_sandbox[_clar_tempdir_len + 5] = alpha[(num & 0x0000f000) >> 12];
	_clar_sandbox[_clar_tempdir_len + 6] = alpha[(num & 0x00000f00) >> 8];
	_clar_sandbox[_clar_tempdir_len + 7] = alpha[(num & 0x000000f0) >> 4];
	_clar_sandbox[_clar_tempdir_len + 8] = alpha[(num & 0x0000000f) >> 0];
	_clar_sandbox[_clar_tempdir_len + 9] = '\0';

	if (mkdir(_clar_sandbox, 0700) != 0)
		return -1;

	if (chdir(_clar_sandbox) != 0)
		return -1;

	return 0;
}

static int clar_sandbox_cleanup(void)
{
	cl_assert(_clar_sandbox[0] != '\0');

	fs_rm(_clar_sandbox);
	_clar_sandbox[0] = '\0';

	if (chdir(_clar_tempdir) != 0)
		return -1;

	return 0;
}

const char *clar_tempdir_path(void)
{
	return _clar_tempdir;
}

const char *clar_sandbox_path(void)
{
	return _clar_sandbox;
}

