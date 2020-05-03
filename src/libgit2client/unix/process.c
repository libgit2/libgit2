/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <stdio.h>
#include <sys/wait.h>
#include <git2.h>

#include "git2_util.h"
#include "vector.h"
#include "process.h"
#include "git2/strarray.h"

extern char **environ;

struct git_process {
	char **args;
	char **env;

	char *cwd;

	unsigned int capture_in  : 1,
	             capture_out : 1,
	             capture_err : 1;

	pid_t pid;

	int child_in;
	int child_out;
	int child_err;
	git_process_result_status status;
};

static void strings_free_deep(char **strings)
{
	char **s;

	if (!strings)
		return;

	for (s = strings; *s; s++)
		git__free(*s);

	git__free(strings);
}

static int strings_copy_with_null(char ***out, const char **in, size_t len)
{
	char **dup;
	size_t new_len, i;

	GIT_ERROR_CHECK_ALLOC_ADD(&new_len, len, 1);

	dup = git__calloc(new_len, sizeof(char *));
	GIT_ERROR_CHECK_ALLOC(dup);

	for (i = 0; i < len; i++) {
		dup[i] = git__strdup(in[i]);
		GIT_ERROR_CHECK_ALLOC(dup[i]);
	}

	*out = dup;
	return 0;
}

GIT_INLINE(bool) is_delete_env(const char *env)
{
	char *c = index(env, '=');

	if (c == NULL)
		return false;

	return *(c+1) == '\0';
}

static int merge_env(char ***out, const char **env, size_t env_len, bool exclude_env)
{
	git_vector merged = GIT_VECTOR_INIT;
	char **kv, *dup;
	size_t max, cnt;
	int error = 0;

	for (max = env_len, kv = environ; !exclude_env && *kv; kv++)
		max++;

	if ((error = git_vector_init(&merged, max, NULL)) < 0)
		goto on_error;

	for (cnt = 0; env && cnt < env_len; cnt++) {
		if (is_delete_env(env[cnt]))
			continue;

		dup = git__strdup(env[cnt]);
		GIT_ERROR_CHECK_ALLOC(dup);

		if ((error = git_vector_insert(&merged, dup)) < 0)
			goto on_error;
	}

	if (!exclude_env) {
		for (kv = environ; *kv; kv++) {
			if (env && git_strings_contains_key(env, env_len, *kv, '='))
				continue;

			dup = git__strdup(*kv);
			GIT_ERROR_CHECK_ALLOC(dup);

			if ((error = git_vector_insert(&merged, dup)) < 0)
				goto on_error;
		}
	}

	if (merged.length == 0) {
		*out = NULL;
		error = 0;
		goto on_error;
	}

	git_vector_insert(&merged, NULL);

	*out = (char **)merged.contents;

	return 0;

on_error:
	git_vector_free_deep(&merged);
	return error;
}

int git_process_new(
	git_process **out,
	const char **args,
	size_t args_len,
	const char **env,
	size_t env_len,
	git_process_options *opts)
{
	git_process *process;

	assert(out && args && args_len > 0);

	*out = NULL;

	process = git__calloc(sizeof(git_process), 1);
	GIT_ERROR_CHECK_ALLOC(process);

	if (strings_copy_with_null(&process->args, args, args_len) < 0 ||
	    merge_env(&process->env, env, env_len, opts->exclude_env) < 0) {
		git_process_free(process);
		return -1;
	}

	if (opts) {
		process->capture_in = opts->capture_in;
		process->capture_out = opts->capture_out;
		process->capture_err = opts->capture_err;

		if (opts->cwd) {
			process->cwd = git__strdup(opts->cwd);
			GIT_ERROR_CHECK_ALLOC(process->cwd);
		}
	}

	process->child_in  = -1;
	process->child_out = -1;
	process->child_err = -1;
	process->status    = -1;

	*out = process;
	return 0;
}

#define CLOSE_FD(fd) \
	if (fd >= 0) {     \
		close(fd); \
		fd = -1;   \
	}

static int try_read(size_t *out, int fd, void *buf, size_t len)
{
	size_t read_len = 0;
	int ret = -1;

	while (ret && read_len < len) {
		ret = read(fd, buf + read_len, len - read_len);

		if (ret < 0 && errno != EAGAIN && errno != EINTR) {
			git_error_set(GIT_ERROR_OS, "could not read child status");
			return -1;
		}

		read_len += ret;
	}

	*out = read_len;
	return 0;
}


static int read_status(int fd)
{
	size_t status_len = sizeof(int) * 3, read_len = 0;
	char buffer[status_len], fn[128];
	int error, fn_error, os_error, fn_len = 0;

	if ((error = try_read(&read_len, fd, buffer, status_len)) < 0)
		return error;

	/* Immediate EOF indicates the exec succeeded. */
	if (read_len == 0)
		return 0;

	if (read_len < status_len) {
		git_error_set(GIT_ERROR_INVALID, "child status truncated");
		return -1;
	}

	memcpy(&fn_error, &buffer[0], sizeof(int));
	memcpy(&os_error, &buffer[sizeof(int)], sizeof(int));
	memcpy(&fn_len, &buffer[sizeof(int) * 2], sizeof(int));

	if (fn_len > 0) {
		fn_len = min(fn_len, (int)(ARRAY_SIZE(fn) - 1));

		if ((error = try_read(&read_len, fd, fn, fn_len)) < 0)
			return error;

		fn[fn_len] = '\0';
	} else {
		fn[0] = '\0';
	}

	if (fn_error) {
		errno = os_error;
		git_error_set(GIT_ERROR_OS, "could not %s", fn[0] ? fn : "(unknown)");
	}

	return fn_error;
}

static bool try_write(int fd, const void *buf, size_t len)
{
	size_t write_len;
	int ret;

	for (write_len = 0; write_len < len; ) {
		ret = write(fd, buf + write_len, len - write_len);

		if (ret <= 0)
			break;

		write_len += ret;
	}

	return (len == write_len);
}

static void write_status(int fd, const char *fn, int error, int os_error)
{
	size_t status_len = sizeof(int) * 3, fn_len;
	char buffer[status_len];

	fn_len = strlen(fn);

	if (fn_len > INT_MAX)
		fn_len = INT_MAX;

	memcpy(&buffer[0], &error, sizeof(int));
	memcpy(&buffer[sizeof(int)], &os_error, sizeof(int));
	memcpy(&buffer[sizeof(int) * 2], &fn_len, sizeof(int));

	/* Do our best effort to write all the status. */
	if (!try_write(fd, buffer, status_len))
		return;

	if (fn_len)
		try_write(fd, fn, fn_len);
}

int git_process_start(git_process *process)
{
	int in[2] = { -1, -1 }, out[2] = { -1, -1 },
	    err[2] = { -1, -1 }, status[2] = { -1, -1 };
	int fdflags, state, error;
	pid_t pid;

	/* Set up the pipes to read from/write to the process */
	if ((process->capture_in && pipe(in) < 0) ||
	    (process->capture_out && pipe(out) < 0) ||
	    (process->capture_err && pipe(err) < 0)) {
		git_error_set(GIT_ERROR_OS, "could not create pipe");
		goto on_error;
	}

	/* Set up a self-pipe for status from the forked process. */
	if (pipe(status) < 0 ||
	    (fdflags = fcntl(status[1], F_GETFD)) < 0 ||
	    fcntl(status[1], F_SETFD, fdflags | FD_CLOEXEC) < 0) {
		git_error_set(GIT_ERROR_OS, "could not create pipe");
		goto on_error;
	}

	switch (pid = fork()) {
	case -1:
		git_error_set(GIT_ERROR_OS, "could not fork");
		goto on_error;

	/* Child: start the process. */
	case 0:
		/* Close the opposing side of the pipes */
		CLOSE_FD(status[0]);

		if (process->capture_in) {
			CLOSE_FD(in[1]);
			dup2(in[0],  STDIN_FILENO);
		}

		if (process->capture_out) {
			CLOSE_FD(out[0]);
			dup2(out[1], STDOUT_FILENO);
		}

		if (process->capture_err) {
			CLOSE_FD(err[0]);
			dup2(err[1], STDERR_FILENO);
		}

		if (process->cwd && (error = chdir(process->cwd)) < 0) {
			write_status(status[1], "chdir", error, errno);
			exit(0);
		}

		/*
		 * Exec the process and write the results back if the
		 * call fails.  If it succeeds, we'll close the status
		 * pipe (via CLOEXEC) and the parent will know.
		 */
		error = execve(process->args[0],
		               process->args,
			       process->env);

		write_status(status[1], "execve", error, errno);
		exit(0);

	/* Parent: make sure the child process exec'd correctly. */
	default:
		/* Close the opposing side of the pipes */
		CLOSE_FD(status[1]);

		if (process->capture_in) {
			CLOSE_FD(in[0]);
			process->child_in  = in[1];
		}

		if (process->capture_out) {
			CLOSE_FD(out[1]);
			process->child_out = out[0];
		}

		if (process->capture_err) {
			CLOSE_FD(err[1]);
			process->child_err = err[0];
		}

		/* Try to read the status */
		process->status = status[0];
		if ((error = read_status(status[0])) < 0) {
			waitpid(process->pid, &state, 0);
			goto on_error;
		}

		process->pid = pid;
		return 0;
	}

on_error:
	CLOSE_FD(in[0]);     CLOSE_FD(in[1]);
	CLOSE_FD(out[0]);    CLOSE_FD(out[1]);
	CLOSE_FD(err[0]);    CLOSE_FD(err[1]);
	CLOSE_FD(status[0]); CLOSE_FD(status[1]);
	return -1;
}

ssize_t git_process_read(git_process *process, void *buf, size_t count)
{
	ssize_t ret;

	assert(process && process->capture_out);

	if (count > SSIZE_MAX)
		count = SSIZE_MAX;

	if ((ret = read(process->child_out, buf, count)) < 0) {
		git_error_set(GIT_ERROR_OS, "could not read from child process");
		return -1;
	}

	return ret;
}

ssize_t git_process_write(git_process *process, const void *buf, size_t count)
{
	ssize_t ret;

	assert(process && process->capture_in);

	if (count > SSIZE_MAX)
		count = SSIZE_MAX;

	if ((ret = write(process->child_in, buf, count)) < 0) {
		git_error_set(GIT_ERROR_OS, "could not write to child process");
		return -1;
	}

	return ret;
}

int git_process_close_in(git_process *process)
{
	if (!process->capture_in) {
		git_error_set(GIT_ERROR_INVALID, "input is not open");
		return -1;
	}

	CLOSE_FD(process->child_in);
	return 0;
}

int git_process_close_out(git_process *process)
{
	if (!process->capture_out) {
		git_error_set(GIT_ERROR_INVALID, "output is not open");
		return -1;
	}

	CLOSE_FD(process->child_out);
	return 0;
}

int git_process_close_err(git_process *process)
{
	if (!process->capture_err) {
		git_error_set(GIT_ERROR_INVALID, "error is not open");
		return -1;
	}

	CLOSE_FD(process->child_err);
	return 0;
}

int git_process_close(git_process *process)
{
	CLOSE_FD(process->child_in);
	CLOSE_FD(process->child_out);
	CLOSE_FD(process->child_err);
	CLOSE_FD(process->status);

	return 0;
}

int git_process_wait(git_process_result *result, git_process *process)
{
	int state;

	if (result)
		memset(result, 0, sizeof(git_process_result));

	if (!process->pid) {
		git_error_set(GIT_ERROR_INVALID, "process is stopped");
		return -1;
	}

	waitpid(process->pid, &state, 0);
	process->pid = 0;

	if (result) {
		if (WIFEXITED(state)) {
			result->status = GIT_PROCESS_STATUS_NORMAL;
			result->exitcode = WEXITSTATUS(state);
		} else if (WIFSIGNALED(state)) {
			result->status = GIT_PROCESS_STATUS_ERROR;
			result->signal = WTERMSIG(state);
		} else {
			result->status = GIT_PROCESS_STATUS_ERROR;
		}
	}

	return 0;
}

int git_process_result_msg(git_buf *out, git_process_result *result)
{
	if (result->status == GIT_PROCESS_STATUS_NONE) {
		return git_buf_puts(out, "process not started");
	} else if (result->status == GIT_PROCESS_STATUS_NORMAL) {
		return git_buf_printf(out, "process exited with code %d",
		                      result->exitcode);
	} else if (result->signal) {
		return git_buf_printf(out, "process exited on signal %d",
		                      result->signal);
	}

	return git_buf_puts(out, "unknown error");
}

void git_process_free(git_process *process)
{
	if (!process)
		return;

	if (process->pid)
		git_process_close(process);

	git__free(process->cwd);
	strings_free_deep(process->args);
	strings_free_deep(process->env);
	git__free(process);
}
