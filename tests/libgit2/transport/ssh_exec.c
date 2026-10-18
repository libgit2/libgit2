#include "clar_libgit2.h"
#include "git2/sys/remote.h"
#include "git2/sys/transport.h"
#include "futils.h"

#ifndef GIT_WIN32
# include <sys/wait.h>
#endif


void test_transport_ssh_exec__reject_injection_username(void)
{
#ifndef GIT_SSH_EXEC
	cl_skip();
#else
	git_remote *remote;
	git_repository *repo;
	git_transport *transport;
	const char *url = "-oProxyCommand=git@somehost:somepath";
	git_remote_connect_options opts = GIT_REMOTE_CONNECT_OPTIONS_INIT;


	cl_git_pass(git_repository_init(&repo, "./transport-username", 0));
	cl_git_pass(git_remote_create(&remote, repo, "test",
				      cl_fixture("testrepo.git")));
	cl_git_pass(git_transport_new(&transport, remote, url));
	cl_git_fail_with(-1, transport->connect(transport, url,
						GIT_SERVICE_UPLOADPACK_LS, &opts));

	transport->free(transport);
	git_remote_free(remote);
	git_repository_free(repo);
#endif
}

void test_transport_ssh_exec__reject_injection_hostname(void)
{
#ifndef GIT_SSH_EXEC
	cl_skip();
#else
	git_remote *remote;
	git_repository *repo;
	git_transport *transport;
	const char *url = "-oProxyCommand=somehost:somepath-hostname";
	git_remote_connect_options opts = GIT_REMOTE_CONNECT_OPTIONS_INIT;


	cl_git_pass(git_repository_init(&repo, "./transport-hostname", 0));
	cl_git_pass(git_remote_create(&remote, repo, "test",
				      cl_fixture("testrepo.git")));
	cl_git_pass(git_transport_new(&transport, remote, url));
	cl_git_fail_with(-1, transport->connect(transport, url,
						GIT_SERVICE_UPLOADPACK_LS, &opts));

	transport->free(transport);
	git_remote_free(remote);
	git_repository_free(repo);
#endif
}

void test_transport_ssh_exec__reject_injection_path(void)
{
#ifndef GIT_SSH_EXEC
	cl_skip();
#else
	git_remote *remote;
	git_repository *repo;
	git_transport *transport;
	const char *url = "git@somehost:-somepath";
	git_remote_connect_options opts = GIT_REMOTE_CONNECT_OPTIONS_INIT;


	cl_git_pass(git_repository_init(&repo, "./transport-path", 0));
	cl_git_pass(git_remote_create(&remote, repo, "test",
				      cl_fixture("testrepo.git")));
	cl_git_pass(git_transport_new(&transport, remote, url));
	cl_git_fail_with(-1, transport->connect(transport, url,
						GIT_SERVICE_UPLOADPACK_LS, &opts));

	transport->free(transport);
	git_remote_free(remote);
	git_repository_free(repo);
#endif
}

void test_transport_ssh_exec__reaps_child_on_disconnect(void)
{
#if !defined(GIT_SSH_EXEC) || defined(GIT_WIN32)
	cl_skip();
#else
	git_repository *repo;
	git_remote *remote;
	git_str pid_file = GIT_STR_INIT;
	char *old_ssh = cl_getenv("GIT_SSH");
	char *old_ssh_command = cl_getenv("GIT_SSH_COMMAND");
	pid_t pid, waited[3];
	int errors[3], status, i;

	cl_git_mkfile("fake-ssh",
		"#!/bin/sh\n"
		"echo $$ > ssh-child.pid\n"
		"printf '0000'\n"
		"exec cat > /dev/null\n");
	cl_must_pass(p_chmod("fake-ssh", 0755));
	cl_setenv("GIT_SSH_COMMAND", NULL);
	cl_setenv("GIT_SSH", "./fake-ssh");

	cl_git_pass(git_repository_init(&repo, "./transport-child", 0));
	cl_git_pass(git_remote_create(&remote, repo, "test", "ssh://localhost/repo"));

	for (i = 0; i < 3; i++) {
		cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH,
			NULL, NULL, NULL));
		cl_git_pass(git_futils_readbuffer(&pid_file, "ssh-child.pid"));
		pid = (pid_t)strtol(pid_file.ptr, NULL, 10);
		cl_assert(pid > 0);

		cl_git_pass(git_remote_disconnect(remote));
		errno = 0;
		waited[i] = waitpid(pid, &status, 0);
		errors[i] = errno;
	}

	git_remote_free(remote);
	git_repository_free(repo);
	git_str_dispose(&pid_file);
	cl_setenv("GIT_SSH", old_ssh);
	cl_setenv("GIT_SSH_COMMAND", old_ssh_command);
	git__free(old_ssh);
	git__free(old_ssh_command);

	for (i = 0; i < 3; i++) {
		cl_assert_equal_i(-1, waited[i]);
		cl_assert_equal_i(ECHILD, errors[i]);
	}
#endif
}
