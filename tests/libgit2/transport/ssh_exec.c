#include "clar_libgit2.h"
#include "git2/sys/remote.h"
#include "git2/sys/transport.h"


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
