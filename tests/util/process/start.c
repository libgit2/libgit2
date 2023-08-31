#include "clar_libgit2.h"
#include "process.h"
#include "vector.h"

#ifndef GIT_WIN32
# include <signal.h>
#endif

#ifndef SIGTERM
# define SIGTERM 42
#endif

#ifndef SIGPIPE
# define SIGPIPE 42
#endif

static git_str helloworld_cmd = GIT_STR_INIT;
static git_str cat_cmd = GIT_STR_INIT;
static git_str pwd_cmd = GIT_STR_INIT;

void test_process_start__initialize(void)
{
#ifdef GIT_WIN32
	git_str_printf(&helloworld_cmd, "%s/helloworld.bat", cl_fixture("process"));
	git_str_printf(&cat_cmd, "%s/cat.bat", cl_fixture("process"));
	git_str_printf(&pwd_cmd, "%s/pwd.bat", cl_fixture("process"));
#else
	git_str_printf(&helloworld_cmd, "%s/helloworld.sh", cl_fixture("process"));
#endif
}

void test_process_start__cleanup(void)
{
	git_str_dispose(&pwd_cmd);
	git_str_dispose(&cat_cmd);
	git_str_dispose(&helloworld_cmd);
}

void test_process_start__returncode(void)
{
#ifdef GIT_WIN32
	const char *args_array[] = { "C:\\Windows\\System32\\cmd.exe", "/c", "exit", "1" };
#elif __APPLE__
	const char *args_array[] = { "/usr/bin/false" };
#else
	const char *args_array[] = { "/bin/false" };
#endif

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_pass(git_process_start(process));
	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_NORMAL, result.status);
	cl_assert_equal_i(1, result.exitcode);
	cl_assert_equal_i(0, result.signal);

	git_process_free(process);
}

void test_process_start__not_found(void)
{
#ifdef GIT_WIN32
	const char *args_array[] = { "C:\\a\\b\\z\\y\\not_found" };
#else
	const char *args_array[] = { "/a/b/z/y/not_found" };
#endif

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_fail(git_process_start(process));
	git_process_free(process);
}

static void write_all(git_process *process, char *buf)
{
	size_t buf_len = strlen(buf);
	ssize_t ret;

	while (buf_len) {
		ret = git_process_write(process, buf, buf_len);
		cl_git_pass(ret < 0 ? (int)ret : 0);

		buf += ret;
		buf_len -= ret;
	}
}

static void read_all(git_str *out, git_process *process)
{
	char buf[32];
	size_t buf_len = 32;
	ssize_t ret;

	while ((ret = git_process_read(process, buf, buf_len)) > 0)
		cl_git_pass(git_str_put(out, buf, ret));

	cl_git_pass(ret < 0 ? (int)ret : 0);
}

void test_process_start__redirect_stdio(void)
{
#ifdef GIT_WIN32
	const char *args_array[] = { "C:\\Windows\\System32\\cmd.exe", "/c", cat_cmd.ptr };
#else
	const char *args_array[] = { "/bin/cat" };
#endif

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	git_str buf = GIT_STR_INIT;

	opts.capture_in = 1;
	opts.capture_out = 1;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_pass(git_process_start(process));

	write_all(process, "Hello, world.\r\nHello!\r\n");
	cl_git_pass(git_process_close_in(process));

	read_all(&buf, process);
	cl_assert_equal_s("Hello, world.\r\nHello!\r\n", buf.ptr);

	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_NORMAL, result.status);
	cl_assert_equal_i(0, result.exitcode);
	cl_assert_equal_i(0, result.signal);

	git_str_dispose(&buf);
	git_process_free(process);
}

/*
void test_process_start__catch_sigterm(void)
{
	const char *args_array[] = { "/bin/cat" };

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	p_pid_t pid;

	opts.capture_out = 1;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_pass(git_process_start(process));
	cl_git_pass(git_process_id(&pid, process));

	cl_must_pass(kill(pid, SIGTERM));

	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_ERROR, result.status);
	cl_assert_equal_i(0, result.exitcode);
	cl_assert_equal_i(SIGTERM, result.signal);

	git_process_free(process);
}

void test_process_start__catch_sigpipe(void)
{
	const char *args_array[] = { helloworld_cmd.ptr };

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;

	opts.capture_out = 1;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_pass(git_process_start(process));
	cl_git_pass(git_process_close(process));
	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_ERROR, result.status);
	cl_assert_equal_i(0, result.exitcode);
	cl_assert_equal_i(SIGPIPE, result.signal);

	git_process_free(process);
}
*/

void test_process_start__can_chdir(void)
{
#ifdef GIT_WIN32
	const char *args_array[] = { "C:\\Windows\\System32\\cmd.exe", "/c", pwd_cmd.ptr };
	char *startwd = "C:\\";
#else
	const char *args_array[] = { "/bin/pwd" };
	char *startwd = "/";
#endif

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	git_str buf = GIT_STR_INIT;

	opts.cwd = startwd;
	opts.capture_out = 1;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_pass(git_process_start(process));

	read_all(&buf, process);
	git_str_rtrim(&buf);

	cl_assert_equal_s(startwd, buf.ptr);

	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_NORMAL, result.status);
	cl_assert_equal_i(0, result.exitcode);
	cl_assert_equal_i(0, result.signal);

	git_str_dispose(&buf);
	git_process_free(process);
}

void test_process_start__cannot_chdir_to_nonexistent_dir(void)
{
#ifdef GIT_WIN32
	const char *args_array[] = { "C:\\Windows\\System32\\cmd.exe", "/c", pwd_cmd.ptr };
	char *startwd = "C:\\a\\b\\z\\y\\not_found";
#else
	const char *args_array[] = { "/bin/pwd" };
	char *startwd = "/a/b/z/y/not_found";
#endif

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;

	opts.cwd = startwd;
	opts.capture_out = 1;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), NULL, 0, &opts));
	cl_git_fail(git_process_start(process));
	git_process_free(process);
}
