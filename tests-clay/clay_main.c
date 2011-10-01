#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* required for sandboxing */
#include <sys/types.h>
#include <sys/stat.h>

#define clay_print(...) printf(__VA_ARGS__)

#ifdef _WIN32
#	include <windows.h>
#	include <io.h>
#	include <shellapi.h>
#	include <direct.h>

#	define _MAIN_CC __cdecl

#	define stat(path, st) _stat(path, st)
#	define mkdir(path, mode) _mkdir(path)
#	define chdir(path) _chdir(path)
#	define access(path, mode) _access(path, mode)
#	define strdup(str) _strdup(str)

#	ifndef __MINGW32__
#		pragma comment(lib, "shell32")
#		define strncpy(to, from, to_size) strncpy_s(to, to_size, from, _TRUNCATE)
#		define W_OK 02
#		define S_ISDIR(x) ((x & _S_IFDIR) != 0)
#		define mktemp_s(path, len) _mktemp_s(path, len)
#	endif
	typedef struct _stat STAT_T;
#else
#	include <sys/wait.h> /* waitpid(2) */
#	include <unistd.h>
#	define _MAIN_CC
	typedef struct stat STAT_T;
#endif

#include "clay.h"

static void fs_rm(const char *_source);
static void fs_copy(const char *_source, const char *dest);

static const char *
fixture_path(const char *base, const char *fixture_name);

struct clay_error {
	const char *test;
	int test_number;
	const char *suite;
	const char *file;
	int line_number;
	const char *error_msg;
	char *description;

	struct clay_error *next;
};

static struct {
	const char *active_test;
	const char *active_suite;

	int suite_errors;
	int total_errors;

	int test_count;

	struct clay_error *errors;
	struct clay_error *last_error;

	void (*local_cleanup)(void *);
	void *local_cleanup_payload;

	jmp_buf trampoline;
	int trampoline_enabled;
} _clay;

struct clay_func {
	const char *name;
	void (*ptr)(void);
	size_t suite_n;
};

struct clay_suite {
	const char *name;
	struct clay_func initialize;
	struct clay_func cleanup;
	const struct clay_func *tests;
	size_t test_count;
};

/* From clay_sandbox.c */
static void clay_unsandbox(void);
static int clay_sandbox(void);

static void
clay_run_test(
	const struct clay_func *test,
	const struct clay_func *initialize,
	const struct clay_func *cleanup)
{
	int error_st = _clay.suite_errors;

	_clay.trampoline_enabled = 1;

	if (setjmp(_clay.trampoline) == 0) {
		if (initialize->ptr != NULL)
			initialize->ptr();

		test->ptr();
	}

	_clay.trampoline_enabled = 0;

	if (_clay.local_cleanup != NULL)
		_clay.local_cleanup(_clay.local_cleanup_payload);

	if (cleanup->ptr != NULL)
		cleanup->ptr();

	_clay.test_count++;

	/* remove any local-set cleanup methods */
	_clay.local_cleanup = NULL;
	_clay.local_cleanup_payload = NULL;

	clay_print("%c", (_clay.suite_errors > error_st) ? 'F' : '.');
}

static void
clay_print_error(int num, const struct clay_error *error)
{
	clay_print("  %d) Failure:\n", num);

	clay_print("%s::%s (%s) [%s:%d] [-t%d]\n",
		error->suite,
		error->test,
		"no description",
		error->file,
		error->line_number,
		error->test_number);

	clay_print("  %s\n", error->error_msg);

	if (error->description != NULL)
		clay_print("  %s\n", error->description);

	clay_print("\n");
}

static void
clay_report_errors(void)
{
	int i = 1;
	struct clay_error *error, *next;

	error = _clay.errors;
	while (error != NULL) {
		next = error->next;
		clay_print_error(i++, error);
		free(error->description);
		free(error);
		error = next;
	}
}

static void
clay_run_suite(const struct clay_suite *suite)
{
	const struct clay_func *test = suite->tests;
	size_t i;

	_clay.active_suite = suite->name;
	_clay.suite_errors = 0;

	for (i = 0; i < suite->test_count; ++i) {
		_clay.active_test = test[i].name;
		clay_run_test(&test[i], &suite->initialize, &suite->cleanup);
	}
}

static void
clay_run_single(const struct clay_func *test,
	const struct clay_suite *suite)
{
	_clay.suite_errors = 0;
	_clay.active_suite = suite->name;
	_clay.active_test = test->name;

	clay_run_test(test, &suite->initialize, &suite->cleanup);
}

static void
clay_usage(const char *arg)
{
	printf("Usage: %s [options]\n\n", arg);
	printf("Options:\n");
	printf("  -tXX\t\tRun only the test number XX\n");
	printf("  -sXX\t\tRun only the suite number XX\n");
	exit(-1);
}

static void
clay_parse_args(
	int argc, char **argv,
	const struct clay_func *callbacks,
	size_t cb_count,
	const struct clay_suite *suites,
	size_t suite_count)
{
	int i;

	for (i = 1; i < argc; ++i) {
		char *argument = argv[i];
		char action;
		int num;

		if (argument[0] != '-')
			clay_usage(argv[0]);

		action = argument[1];
		num = strtol(argument + 2, &argument, 10);

		if (*argument != '\0' || num < 0)
			clay_usage(argv[0]);

		switch (action) {
		case 't':
			if ((size_t)num >= cb_count) {
				fprintf(stderr, "Test number %d does not exist.\n", num);
				exit(-1);
			}

			clay_print("Started (%s::%s)\n",
				suites[callbacks[num].suite_n].name,
				callbacks[num].name);

			clay_run_single(&callbacks[num], &suites[callbacks[num].suite_n]);
			break;

		case 's':
			if ((size_t)num >= suite_count) {
				fprintf(stderr, "Suite number %d does not exist.\n", num);
				exit(-1);
			}

			clay_print("Started (%s::*)\n", suites[num].name);
			clay_run_suite(&suites[num]);
			break;

		default:
			clay_usage(argv[0]);
		}
	}
}

static int
clay_test(
	int argc, char **argv,
	const char *suites_str,
	const struct clay_func *callbacks,
	size_t cb_count,
	const struct clay_suite *suites,
	size_t suite_count)
{
	clay_print("Loaded %d suites: %s\n", (int)suite_count, suites_str);

	if (clay_sandbox() < 0) {
		fprintf(stderr,
			"Failed to sandbox the test runner.\n"
			"Testing will proceed without sandboxing.\n");
	}

	if (argc > 1) {
		clay_parse_args(argc, argv,
			callbacks, cb_count, suites, suite_count);

	} else {
		size_t i;
		clay_print("Started\n");

		for (i = 0; i < suite_count; ++i) {
			const struct clay_suite *s = &suites[i];
			clay_run_suite(s);
		}
	}

	clay_print("\n\n");
	clay_report_errors();

	clay_unsandbox();
	return _clay.total_errors;
}

void
clay__assert(
	int condition,
	const char *file,
	int line,
	const char *error_msg,
	const char *description,
	int should_abort)
{
	struct clay_error *error;

	if (condition)
		return;

	error = calloc(1, sizeof(struct clay_error));

	if (_clay.errors == NULL)
		_clay.errors = error;

	if (_clay.last_error != NULL)
		_clay.last_error->next = error;

	_clay.last_error = error;

	error->test = _clay.active_test;
	error->test_number = _clay.test_count;
	error->suite = _clay.active_suite;
	error->file = file;
	error->line_number = line;
	error->error_msg = error_msg;

	if (description != NULL)
		error->description = strdup(description);

	_clay.suite_errors++;
	_clay.total_errors++;

	if (should_abort) {
		if (!_clay.trampoline_enabled) {
			fprintf(stderr,
				"Fatal error: a cleanup method raised an exception.");
			exit(-1);
		}

		longjmp(_clay.trampoline, -1);
	}
}

void cl_set_cleanup(void (*cleanup)(void *), void *opaque)
{
	_clay.local_cleanup = cleanup;
	_clay.local_cleanup_payload = opaque;
}

static char _clay_path[4096];

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
	static const size_t var_count = 4;
	static const char *env_vars[] = {
		"TMPDIR", "TMP", "TEMP", "USERPROFILE"
 	};

 	size_t i;

	for (i = 0; i < var_count; ++i) {
		const char *env = getenv(env_vars[i]);
		if (!env)
			continue;

		if (is_valid_tmp_path(env)) {
			strncpy(buffer, env, length);
			return 0;
		}
	}

	/* If the environment doesn't say anything, try to use /tmp */
	if (is_valid_tmp_path("/tmp")) {
		strncpy(buffer, "/tmp", length);
		return 0;
	}

#else
	if (GetTempPath((DWORD)length, buffer))
		return 0;
#endif

	/* This system doesn't like us, try to use the current directory */
	if (is_valid_tmp_path(".")) {
		strncpy(buffer, ".", length);
		return 0;
	}

	return -1;
}

static void clay_unsandbox(void)
{
	if (_clay_path[0] == '\0')
		return;

#ifdef _WIN32
	chdir("..");
#endif

	fs_rm(_clay_path);
}

static int build_sandbox_path(void)
{
	const char path_tail[] = "clay_tmp_XXXXXX";
	size_t len;

	if (find_tmp_path(_clay_path, sizeof(_clay_path)) < 0)
		return -1;

	len = strlen(_clay_path);

#ifdef _WIN32
	{ /* normalize path to POSIX forward slashes */
		size_t i;
		for (i = 0; i < len; ++i) {
			if (_clay_path[i] == '\\')
				_clay_path[i] = '/';
		}
	}
#endif

	if (_clay_path[len - 1] != '/') {
		_clay_path[len++] = '/';
	}

	strncpy(_clay_path + len, path_tail, sizeof(_clay_path) - len);

#ifdef _WIN32
	if (mktemp_s(_clay_path, sizeof(_clay_path)) != 0)
		return -1;

	if (mkdir(_clay_path, 0700) != 0)
		return -1;
#else
	if (mkdtemp(_clay_path) == NULL)
		return -1;
#endif

	return 0;
}

static int clay_sandbox(void)
{
	if (_clay_path[0] == '\0' && build_sandbox_path() < 0)
		return -1;

	if (chdir(_clay_path) != 0)
		return -1;

	return 0;
}


static const char *
fixture_path(const char *base, const char *fixture_name)
{
	static char _path[4096];
	size_t root_len;

	root_len = strlen(base);
	strncpy(_path, base, sizeof(_path));

	if (_path[root_len - 1] != '/')
		_path[root_len++] = '/';

	if (fixture_name[0] == '/')
		fixture_name++;

	strncpy(_path + root_len,
		fixture_name,
		sizeof(_path) - root_len);

	return _path;
}

#ifdef CLAY_FIXTURE_PATH
const char *cl_fixture(const char *fixture_name)
{
	return fixture_path(CLAY_FIXTURE_PATH, fixture_name);
}

void cl_fixture_sandbox(const char *fixture_name)
{
	fs_copy(cl_fixture(fixture_name), _clay_path);
}

void cl_fixture_cleanup(const char *fixture_name)
{
	fs_rm(fixture_path(_clay_path, fixture_name));
}
#endif

#ifdef _WIN32

#define FOF_FLAGS (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)

static char *
fileops_path(const char *_path)
{
	char *path = NULL;
	size_t length, i;

	if (_path == NULL)
		return NULL;

	length = strlen(_path);
	path = malloc(length + 2);

	if (path == NULL)
		return NULL;

	memcpy(path, _path, length);
	path[length] = 0;
	path[length + 1] = 0;

	for (i = 0; i < length; ++i) {
		if (path[i] == '/')
			path[i] = '\\';
	}

	return path;
}

static void
fileops(int mode, const char *_source, const char *_dest)
{
	SHFILEOPSTRUCT fops;

	char *source = fileops_path(_source);
	char *dest = fileops_path(_dest);

	ZeroMemory(&fops, sizeof(SHFILEOPSTRUCT));

	fops.wFunc = mode;
	fops.pFrom = source;
	fops.pTo = dest;
	fops.fFlags = FOF_FLAGS;

	cl_assert_(
		SHFileOperation(&fops) == 0,
		"Windows SHFileOperation failed"
	);

	free(source);
	free(dest);
}

static void
fs_rm(const char *_source)
{
	fileops(FO_DELETE, _source, NULL);
}

static void
fs_copy(const char *_source, const char *_dest)
{
	fileops(FO_COPY, _source, _dest);
}

void
cl_fs_cleanup(void)
{
	fs_rm(fixture_path(_clay_path, "*"));
}

#else
static int
shell_out(char * const argv[])
{
	int status;
	pid_t pid;

	pid = fork();

	if (pid < 0) {
		fprintf(stderr,
			"System error: `fork()` call failed.\n");
		exit(-1);
	}

	if (pid == 0) {
		execv(argv[0], argv);
	}

	waitpid(pid, &status, 0);
	return WEXITSTATUS(status);
}

static void
fs_copy(const char *_source, const char *dest)
{
	char *argv[5];
	char *source;
	size_t source_len;

	source = strdup(_source);
	source_len = strlen(source);

	if (source[source_len - 1] == '/')
		source[source_len - 1] = 0;

	argv[0] = "/bin/cp";
	argv[1] = "-R";
	argv[2] = source;
	argv[3] = (char *)dest;
	argv[4] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to copy test fixtures to sandbox"
	);

	free(source);
}

static void
fs_rm(const char *source)
{
	char *argv[4];

	argv[0] = "/bin/rm";
	argv[1] = "-Rf";
	argv[2] = (char *)source;
	argv[3] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to cleanup the sandbox"
	);
}

void
cl_fs_cleanup(void)
{
	clay_unsandbox();
	clay_sandbox();
}
#endif


static const struct clay_func _all_callbacks[] = {
    {"dont_traverse_dot", &test_core_dirent__dont_traverse_dot, 0},
	{"traverse_subfolder", &test_core_dirent__traverse_subfolder, 0},
	{"traverse_slash_terminated_folder", &test_core_dirent__traverse_slash_terminated_folder, 0},
	{"dont_traverse_empty_folders", &test_core_dirent__dont_traverse_empty_folders, 0},
	{"traverse_weird_filenames", &test_core_dirent__traverse_weird_filenames, 0},
	{"0", &test_core_filebuf__0, 1},
	{"1", &test_core_filebuf__1, 1},
	{"2", &test_core_filebuf__2, 1},
	{"0", &test_core_path__0, 2},
	{"1", &test_core_path__1, 2},
	{"2", &test_core_path__2, 2},
	{"5", &test_core_path__5, 2},
	{"6", &test_core_path__6, 2},
	{"delete_recursive", &test_core_rmdir__delete_recursive, 3},
	{"fail_to_delete_non_empty_dir", &test_core_rmdir__fail_to_delete_non_empty_dir, 3},
	{"0", &test_core_string__0, 4},
	{"1", &test_core_string__1, 4},
	{"0", &test_core_vector__0, 5},
	{"1", &test_core_vector__1, 5},
	{"2", &test_core_vector__2, 5},
	{"parsing", &test_network_remotes__parsing, 6},
	{"refspec_parsing", &test_network_remotes__refspec_parsing, 6},
	{"fnmatch", &test_network_remotes__fnmatch, 6},
	{"transform", &test_network_remotes__transform, 6},
	{"hash_single_file", &test_status_single__hash_single_file, 7},
	{"whole_repository", &test_status_worktree__whole_repository, 8},
	{"empty_repository", &test_status_worktree__empty_repository, 8}
};

static const struct clay_suite _all_suites[] = {
    {
        "core::dirent",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[0], 5
    },
	{
        "core::filebuf",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[5], 3
    },
	{
        "core::path",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[8], 5
    },
	{
        "core::rmdir",
        {"initialize", &test_core_rmdir__initialize, 3},
        {NULL, NULL, 0},
        &_all_callbacks[13], 2
    },
	{
        "core::string",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[15], 2
    },
	{
        "core::vector",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[17], 3
    },
	{
        "network::remotes",
        {"initialize", &test_network_remotes__initialize, 6},
        {"cleanup", &test_network_remotes__cleanup, 6},
        &_all_callbacks[20], 4
    },
	{
        "status::single",
        {NULL, NULL, 0},
        {NULL, NULL, 0},
        &_all_callbacks[24], 1
    },
	{
        "status::worktree",
        {"initialize", &test_status_worktree__initialize, 8},
        {"cleanup", &test_status_worktree__cleanup, 8},
        &_all_callbacks[25], 2
    }
};

static const char _suites_str[] = "core::dirent, core::filebuf, core::path, core::rmdir, core::string, core::vector, network::remotes, status::single, status::worktree";

int _MAIN_CC main(int argc, char *argv[])
{
    return clay_test(
        argc, argv, _suites_str,
        _all_callbacks, 27,
        _all_suites, 9
    );
}
