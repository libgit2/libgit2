#include "clar_libgit2.h"
#include "process.h"
#include "vector.h"

static git_str env_cmd = GIT_STR_INIT;
static git_str accumulator = GIT_STR_INIT;
static git_vector env_result = GIT_VECTOR_INIT;

void test_process_env__initialize(void)
{
#ifdef GIT_WIN32
	git_str_printf(&env_cmd, "%s/env.cmd", cl_fixture("process"));
#else
	git_str_puts(&env_cmd, "/usr/bin/env");
#endif

	cl_git_pass(git_vector_init(&env_result, 32, git__strcmp_cb));
}

void test_process_env__cleanup(void)
{
	git_vector_free(&env_result);
	git_str_dispose(&accumulator);
	git_str_dispose(&env_cmd);
}

static void run_env(const char **env_array, size_t env_len, bool exclude_env)
{
	const char *args_array[] = { env_cmd.ptr };

	git_process *process;
	git_process_options opts = GIT_PROCESS_OPTIONS_INIT;
	git_process_result result = GIT_PROCESS_RESULT_INIT;

	char buf[1024], *tok;
	ssize_t ret;

	opts.capture_out = 1;
	opts.exclude_env = exclude_env;

	cl_git_pass(git_process_new(&process, args_array, ARRAY_SIZE(args_array), env_array, env_len, &opts));
	cl_git_pass(git_process_start(process));

	while ((ret = git_process_read(process, buf, 1024)) > 0)
		cl_git_pass(git_str_put(&accumulator, buf, (size_t)ret));

	cl_assert_equal_i(0, ret);

	cl_git_pass(git_process_wait(&result, process));

	cl_assert_equal_i(GIT_PROCESS_STATUS_NORMAL, result.status);
	cl_assert_equal_i(0, result.exitcode);
	cl_assert_equal_i(0, result.signal);

	for (tok = strtok(accumulator.ptr, "\n"); tok; tok = strtok(NULL, "\n")) {
#ifdef GIT_WIN32
		if (strlen(tok) && tok[strlen(tok) - 1] == '\r')
			tok[strlen(tok) - 1] = '\0';
#endif

		cl_git_pass(git_vector_insert(&env_result, tok));
	}

	git_process_close(process);
	git_process_free(process);
}

void test_process_env__can_add_env(void)
{
	const char *env_array[] = { "TEST_NEW_ENV=added", "TEST_OTHER_ENV=also_added" };
	run_env(env_array, 2, false);

	cl_git_pass(git_vector_search(NULL, &env_result, "TEST_NEW_ENV=added"));
	cl_git_pass(git_vector_search(NULL, &env_result, "TEST_OTHER_ENV=also_added"));
}

void test_process_env__can_propagate_env(void)
{
	cl_setenv("TEST_NEW_ENV", "propagated");
	run_env(NULL, 0, false);

	cl_git_pass(git_vector_search(NULL, &env_result, "TEST_NEW_ENV=propagated"));
}

void test_process_env__can_remove_env(void)
{
	const char *env_array[] = { "TEST_NEW_ENV=" };
	char *str;
	size_t i;

	cl_setenv("TEST_NEW_ENV", "propagated");
	run_env(env_array, 1, false);

	git_vector_foreach(&env_result, i, str)
		cl_assert(git__prefixcmp(str, "TEST_NEW_ENV=") != 0);
}

void test_process_env__can_clear_env(void)
{
	const char *env_array[] = { "TEST_NEW_ENV=added", "TEST_OTHER_ENV=also_added" };

	cl_setenv("SOME_EXISTING_ENV", "propagated");
	run_env(env_array, 2, true);

	/*
	 * We can't simply test that the environment is precisely what we
	 * provided.  Some systems (eg win32) will add environment variables
	 * to all processes.
	 */
	cl_assert_equal_i(GIT_ENOTFOUND, git_vector_search(NULL, &env_result, "SOME_EXISTING_ENV=propagated"));
}
