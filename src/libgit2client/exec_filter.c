/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

 #include <stdio.h>
 #include <git2.h>
 #include <git2client.h>
 #include <git2/sys/filter.h>

 #include "git2_util.h"
 #include "process.h"

 #define EXEC_FILTER_NAME "exec"

typedef struct {
	git_filter parent;
} exec_filter;

typedef struct {
	git_writestream parent;
	exec_filter *filter;
	const char *cmd;
	git_writestream *next;
	git_filter_mode_t mode;
	git_process *process;
} exec_filter_stream;

static int exec_filter_check(
	git_filter *f,
	void **payload,
	const git_filter_source *src,
	const char **attr_values)
{
	exec_filter *filter = (exec_filter *)f;
	git_config *config = NULL;
	git_buf configkey = GIT_BUF_INIT, filepath = GIT_BUF_INIT,
		cmdline = GIT_BUF_INIT;
	const char *replacements[][2] = { { "%f", NULL } };
	const char *direction;
	int error;

	GIT_UNUSED(filter);
	GIT_UNUSED(payload);
	GIT_UNUSED(src);

	/* TODO: support `process` */
	if (git_filter_source_mode(src) == GIT_FILTER_SMUDGE)
		direction = "smudge";
	else
		direction = "clean";

	if ((error = git_repository_config_snapshot(&config, git_filter_source_repo(src))) < 0 ||
	    (error = git_buf_printf(&configkey, "filter.%s.%s", attr_values[0], direction)) < 0)
		goto done;

	if ((error = git_config_get_string_buf((git_userbuf *)&cmdline, config, configkey.ptr)) == GIT_ENOTFOUND) {
		git_error_clear();
		error = GIT_PASSTHROUGH;
		goto done;
	} else if (error < 0) {
		goto done;
	}

	if ((error = git_buf_puts(&filepath, git_filter_source_path(src))) < 0 ||
	    (error = git_buf_shellquote(&filepath)) < 0)
	    goto done;

	replacements[0][1] = filepath.ptr;

	if ((error = git_buf_replace(&cmdline, replacements, 1)) < 0)
		goto done;

	*payload = git_buf_detach(&cmdline);

done:
	git_buf_dispose(&cmdline);
	git_buf_dispose(&filepath);
	git_buf_dispose(&configkey);
	git_config_free(config);
	return error;
}

static int exec_filter_stream_write(
	git_writestream *s,
	const char *buffer,
	size_t len)
{
	exec_filter_stream *stream = (exec_filter_stream *)s;

	while (len) {
		int chunk_len = len < INT_MAX ? (int)len : INT_MAX;
		ssize_t ret = git_process_write(stream->process, buffer, chunk_len);

		if (ret < INT_MIN)
			return -1;
		else if (ret < 0)
			return (int)ret;

		len -= ret;
	}

	return 0;
}

static int exec_filter_stream_close(git_writestream *s)
{
	exec_filter_stream *stream = (exec_filter_stream *)s;
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	git_buf process_msg = GIT_BUF_INIT;
	ssize_t ret = -1;
	int error = 0;

	char buffer[1024];
	size_t buffer_len = 1024;

	while (ret) {
		ret = git_process_read(stream->process, buffer, buffer_len);

		if (ret > 0)
			ret = stream->next->write(stream->next, buffer, (size_t)ret);

		if (ret < INT_MIN) {
			error = -1;
			goto done;
		} else if (ret < 0) {
			error = (int)ret;
			goto done;
		}
	}

	if ((error = git_process_wait(&result, stream->process)) < 0)
		goto done;

	if (result.status != GIT_PROCESS_STATUS_NORMAL || result.exitcode) {
		if (git_process_result_msg(&process_msg, &result) == 0)
			git_error_set(GIT_ERROR_CLIENT,
			              "external filter '%s' failed: %s",
			              stream->cmd, process_msg.ptr);

		error = -1;
		goto done;
	}

done:
	stream->next->close(stream->next);

	git_buf_dispose(&process_msg);
	return error;
}

static void exec_filter_stream_free(git_writestream *s)
{
	exec_filter_stream *stream = (exec_filter_stream *)s;
	git__free(stream);
}

static int exec_filter_stream_start(exec_filter_stream *stream)
{
	git_process_options process_opts = GIT_PROCESS_OPTIONS_INIT;
	const char *cmd[3] = { "/bin/sh", "-c", stream->cmd };
	int error;

	process_opts.cwd = "/Users/ethomson/libgit2/git2-cli-refactor-madness-2/build/lfs";

	process_opts.capture_in = 1;
	process_opts.capture_out = 1;

	if ((error = git_process_new(&stream->process, cmd, 3, NULL, 0, &process_opts)) < 0 ||
	    (error = git_process_start(stream->process)) < 0) {
	    git_process_free(stream->process);
	    stream->process = NULL;
	}

	return error;
}

static int exec_filter_stream_init(
	git_writestream **out,
	git_filter *f,
	void **payload,
	const git_filter_source *src,
	git_writestream *next)
{
	exec_filter *filter = (exec_filter *)f;
	exec_filter_stream *stream;
	int error;

	stream = git__calloc(1, sizeof(exec_filter_stream));
	GIT_ERROR_CHECK_ALLOC(stream);

	stream->parent.write = exec_filter_stream_write;
	stream->parent.close = exec_filter_stream_close;
	stream->parent.free = exec_filter_stream_free;
	stream->filter = filter;
	stream->cmd = (const char *)*payload;
	stream->mode = git_filter_source_mode(src);
	stream->next = next;

	if ((error = exec_filter_stream_start(stream)) < 0) {
		git__free(stream);
		return error;
	}

	*out = &stream->parent;
	return 0;
}

static void exec_filter_cleanup(git_filter *f, void *payload)
{
	GIT_UNUSED(f);

	git__free(payload);
}

static void exec_filter_free(git_filter *f)
{
	exec_filter *filter = (exec_filter *)f;
	git__free(filter);
}

int git_exec_filter_register(void)
{
	exec_filter *filter = git__calloc(1, sizeof(exec_filter));
	int error;

	GIT_ERROR_CHECK_ALLOC(filter);

	filter->parent.version = GIT_FILTER_VERSION;
	filter->parent.attributes = "filter=*";
	filter->parent.check = exec_filter_check;
	filter->parent.stream = exec_filter_stream_init;
	filter->parent.cleanup = exec_filter_cleanup;
	filter->parent.shutdown = exec_filter_free;

	error = git_filter_register(EXEC_FILTER_NAME,
	                            &filter->parent,
	                            GIT_FILTER_DRIVER_PRIORITY);

	if (error < 0)
		git__free(filter);

	return error;
}
