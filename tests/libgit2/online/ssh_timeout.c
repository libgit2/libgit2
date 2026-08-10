#include "clar_libgit2.h"
#include "git2/sys/transport.h"
#include "thread.h"

#ifndef _WIN32
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <pthread.h>
# include <unistd.h>
# include <time.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

extern int git_socket_stream__timeout;

#ifdef GIT_SSH_LIBSSH2

#ifdef _WIN32
static SOCKET server_socket = INVALID_SOCKET;
#else
static int server_socket = -1;
#endif
static int server_port = 0;
#ifndef _WIN32
static pthread_t server_thread;
#else
static HANDLE server_thread;
#endif
static git_atomic32 server_running;

/* Black hole server: accepts connections but never responds */
#ifdef _WIN32
static DWORD WINAPI blackhole_server(LPVOID param)
{
	SOCKET client_socket;
	struct sockaddr_in client_addr;
	int client_len = sizeof(client_addr);

	GIT_UNUSED(param);

	git_atomic32_set(&server_running, 1);

	while (git_atomic32_get(&server_running)) {
		client_socket = accept(server_socket,
			(struct sockaddr *)&client_addr, &client_len);
		if (client_socket == INVALID_SOCKET)
			break;

		/* Accept the connection but never send data - this will
		 * cause SSH handshake to timeout */
		Sleep(10000);  /* 10 seconds */
		closesocket(client_socket);
	}

	return 0;
}
#else
static void *blackhole_server(void *param)
{
	int client_socket;
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	GIT_UNUSED(param);

	git_atomic32_set(&server_running, 1);

	while (git_atomic32_get(&server_running)) {
		client_socket = accept(server_socket,
			(struct sockaddr *)&client_addr, &client_len);
		if (client_socket < 0)
			break;

		/* Accept the connection but never send data - this will
		 * cause SSH handshake to timeout */
		sleep(10);  /* 10 seconds */
		close(client_socket);
	}

	return NULL;
}
#endif

static int start_blackhole_server(void)
{
	struct sockaddr_in addr;
#ifdef _WIN32
	int addr_len = sizeof(addr);
#else
	socklen_t addr_len = sizeof(addr);
#endif
	int opt = 1;

#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
	if (server_socket == INVALID_SOCKET)
		return -1;
#else
	if (server_socket < 0)
		return -1;
#endif

#ifdef _WIN32
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		(const char *)&opt, sizeof(opt));
#else
	setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
		&opt, sizeof(opt));
#endif

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = 0;  /* Let OS choose port */

	if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
		return -1;
	}

	if (listen(server_socket, 5) < 0) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
		return -1;
	}

	/* Get the actual port assigned */
	if (getsockname(server_socket, (struct sockaddr *)&addr, &addr_len) < 0) {
#ifdef _WIN32
		closesocket(server_socket);
#else
		close(server_socket);
#endif
		return -1;
	}
	server_port = ntohs(addr.sin_port);

	/* Start server thread */
#ifdef _WIN32
	server_thread = CreateThread(NULL, 0, blackhole_server, NULL, 0, NULL);
	if (server_thread == NULL) {
		closesocket(server_socket);
		return -1;
	}
#else
	if (pthread_create(&server_thread, NULL, blackhole_server, NULL) != 0) {
		close(server_socket);
		return -1;
	}
#endif

	return 0;
}

static void stop_blackhole_server(void)
{
	git_atomic32_set(&server_running, 0);

#ifdef _WIN32
	if (server_socket != INVALID_SOCKET) {
		closesocket(server_socket);
		if (server_thread)
			WaitForSingleObject(server_thread, INFINITE);
		server_socket = INVALID_SOCKET;
	}
	WSACleanup();
#else
	if (server_socket >= 0) {
		close(server_socket);
		pthread_join(server_thread, NULL);
		server_socket = -1;
	}
#endif
}

#endif /* GIT_SSH_LIBSSH2 */

/*
 * Test that SSH connection timeout doesn't cause infinite retry loop.
 *
 * This test creates a TCP server that accepts connections but never
 * responds to SSH handshake, causing libssh2 to timeout.
 *
 * Before the fix: The code would retry indefinitely on LIBSSH2_ERROR_TIMEOUT
 * After the fix: The code properly returns an error after first timeout
 */
void test_online_ssh_timeout__no_infinite_loop(void)
{
#ifndef GIT_SSH_LIBSSH2
	cl_skip();
#else
	git_remote *remote = NULL;
	git_repository *repo = NULL;
	git_transport *transport = NULL;
	git_remote_connect_options opts = GIT_REMOTE_CONNECT_OPTIONS_INIT;
	char url[256];
	int old_timeout;
	clock_t start, end;
	double elapsed_ms;

	/* Start black hole server */
	cl_git_pass(start_blackhole_server());

	/* Create URL to our black hole server */
	sprintf(url, "ssh://localhost:%d/test.git", server_port);

	/* Set a short timeout (100ms) */
	old_timeout = git_socket_stream__timeout;
	git_socket_stream__timeout = 100;

	cl_git_pass(git_repository_init(&repo, "./transport-timeout", 0));
	cl_git_pass(git_remote_create(&remote, repo, "test", url));

	/* Get transport */
	cl_git_pass(git_transport_new(&transport, remote, url));

	/* Attempt connection - should fail due to timeout */
	start = clock();
	cl_git_fail(transport->connect(transport, url,
					GIT_SERVICE_UPLOADPACK_LS, &opts));
	end = clock();

	/* Calculate elapsed time in milliseconds */
	elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

	/* With the fix, this should fail relatively quickly (within 2 seconds).
	 * Without the fix, it would loop many times and take much longer.
	 * We use a generous timeout of 5 seconds to avoid flakiness. */
	cl_assert(elapsed_ms < 5000);

	/* Cleanup */
	transport->free(transport);
	git_remote_free(remote);
	git_repository_free(repo);
	git_socket_stream__timeout = old_timeout;

	stop_blackhole_server();
#endif
}
