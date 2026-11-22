/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "ssh_exec.h"

#ifdef GIT_SSH_EXEC

#include "common.h"

#include "config.h"
#include "net.h"
#include "path.h"
#include "futils.h"
#include "process.h"
#include "transports/smart.h"

typedef struct {
	git_transport_smart_stream parent;
	git_process *process;
} ssh_exec_stream;

static int ssh_exec_stream_read(
	git_transport_smart_stream *s,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	ssh_exec_stream *stream = (ssh_exec_stream *)s;
	ssize_t ret;

	GIT_ASSERT_ARG(stream);

	if ((ret = git_process_read(stream->process, buffer, buf_size)) < 0) {
		return (int)ret;
	}

	*bytes_read = (size_t)ret;
	return 0;
}

static int ssh_exec_stream_write(
        git_transport_smart_stream *s,
        const char *buffer,
        size_t len)
{
	ssh_exec_stream *stream = (ssh_exec_stream *)s;
	ssize_t ret;

	GIT_ASSERT(stream);

	while (len > 0) {
		if ((ret = git_process_write(stream->process, buffer, len)) < 0)
			return (int)ret;

		len -= ret;
	}

	return 0;
}

static void ssh_exec_stream_free(git_transport_smart_stream *s)
{
	ssh_exec_stream *stream = (ssh_exec_stream *)s;

	git_process_close(stream->process);
	git_process_free(stream->process);
	git__free(stream);
}

static int get_ssh_cmdline(
	git_vector *args,
	bool *use_shell,
	git_transport_smart_base *transport,
	git_net_url *url,
	const char *command)
{
	git_remote *remote = transport->session->owner;
	git_repository *repo = remote->repo;
	git_config *cfg;
	git_str ssh_cmd = GIT_STR_INIT, url_and_host = GIT_STR_INIT,
		remote_cmd = GIT_STR_INIT;
	const char *default_ssh_cmd = "ssh";
	int error;

	/*
	 * Safety check: like git, we forbid paths that look like an
	 * option as that could lead to injection to ssh that can make
	 * us do unexpected things
	 */
	if (git_process__is_cmdline_option(url->username)) {
		git_error_set(GIT_ERROR_NET, "cannot ssh: username '%s' is ambiguous with command-line option", url->username);
		return -1;
	} else if (git_process__is_cmdline_option(url->host)) {
		git_error_set(GIT_ERROR_NET, "cannot ssh: host '%s' is ambiguous with command-line option", url->host);
		return -1;
	} else if (git_process__is_cmdline_option(url->path)) {
		git_error_set(GIT_ERROR_NET, "cannot ssh: path '%s' is ambiguous with command-line option", url->path);
		return -1;
	}

	if ((error = git_repository_config_snapshot(&cfg, repo)) < 0)
		return error;

	if ((error = git__getenv(&ssh_cmd, "GIT_SSH_COMMAND")) == 0)
		*use_shell = true;
	else if (error != GIT_ENOTFOUND)
		goto done;
	else if ((error = git__getenv(&ssh_cmd, "GIT_SSH")) == 0)
		*use_shell = false;
	else if (error != GIT_ENOTFOUND)
		goto done;
	else if ((error = git_config__get_string_buf(&ssh_cmd, cfg, "core.sshcommand")) < 0 && error != GIT_ENOTFOUND)
		goto done;

	git_error_clear();

	if (!ssh_cmd.size &&
	    git_str_puts(&ssh_cmd, default_ssh_cmd) < 0)
		goto done;

	if ((error = git_vector_insert(args, git_str_detach(&ssh_cmd))) < 0)
		goto done;

	if (url->port_specified) {
		char *p = git__strdup("-p");
		char *port = git__strdup(url->port);

		if (!p || !port ||
		    (error = git_vector_insert(args, p)) < 0 ||
		    (error = git_vector_insert(args, port)) < 0)
			goto done;
	}

	if (url->username) {
		if ((error = git_str_puts(&url_and_host, url->username)) < 0 ||
		    (error = git_str_putc(&url_and_host, '@')) < 0)
			goto done;
	}

	if ((error = git_str_puts(&url_and_host, url->host)) < 0 ||
	    (error = git_vector_insert(args, git_str_detach(&url_and_host))) < 0)
		goto done;

	if ((error = git_str_puts(&remote_cmd, command)) < 0 ||
	    (error = git_str_puts(&remote_cmd, " '")) < 0 ||
	    (error = git_str_puts_escaped(&remote_cmd, url->path, "'!", "'\\", "'")) < 0 ||
	    (error = git_str_puts(&remote_cmd, "'")) < 0 ||
	    (error = git_vector_insert(args, git_str_detach(&remote_cmd))) < 0)
		goto done;

done:
	git_str_dispose(&ssh_cmd);
	git_str_dispose(&url_and_host);
	git_str_dispose(&remote_cmd);
	git_config_free(cfg);
	return error;
}

int git_ssh_exec__open(
	git_transport_smart_stream **out,
	git_transport_smart_base *transport,
	const char *sshpath,
	const char *command)
{
	const char *env[] = { "GIT_DIR=" };

	git_process_options process_opts = GIT_PROCESS_OPTIONS_INIT;
	git_net_url url = GIT_NET_URL_INIT;
	git_vector args = GIT_VECTOR_INIT;
	ssh_exec_stream *stream = NULL;
	bool use_shell = false;
	int error;

	process_opts.capture_in = 1;
	process_opts.capture_out = 1;
	process_opts.capture_err = 0;

	if (git_net_str_is_url(sshpath))
		error = git_net_url_parse(&url, sshpath);
	else
		error = git_net_url_parse_scp(&url, sshpath);

	if (error < 0)
		goto done;

	if ((error = get_ssh_cmdline(&args, &use_shell,
			transport, &url, command)) < 0)
		goto done;

	process_opts.use_shell = use_shell;

	if ((stream = git__calloc(1, sizeof(ssh_exec_stream))) == NULL) {
		error = -1;
		goto done;
	}

	stream->parent.read = ssh_exec_stream_read;
	stream->parent.write = ssh_exec_stream_write;
	stream->parent.free = ssh_exec_stream_free;

	if ((error = git_process_new(&stream->process,
	     (const char **)args.contents, args.length,
	     env, ARRAY_SIZE(env), &process_opts)) < 0 ||
	    (error = git_process_start(stream->process)) < 0) {
		git_process_free(stream->process);
		git__free(stream);
		goto done;
	}

	*out = &stream->parent;

done:
	git_vector_dispose_deep(&args);
	git_net_url_dispose(&url);
	return error;
}

#endif
