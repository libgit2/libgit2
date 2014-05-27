#include "clar_libgit2.h"
#include "transports/smart.h"
#include "server.h"

static git_server *g_server;
static git_pkt *g_pkt;

void test_network_server__cleanup(void)
{
	git_pkt_free(g_pkt);
	git_server_free(g_server);
}

void test_network_server__request_type(void)
{
	const char buf[] = "0032git-upload-pack /project.git\0host=myserver.com\0";
	const char *rest;
	size_t buflen = sizeof(buf);

	cl_git_pass(git_server_new(&g_server, 0));
	cl_git_pass(git_pkt_parse_line(&g_pkt, buf, &rest, buflen));
	cl_git_pass(git_server__handle_request(g_server, g_pkt));

	cl_assert_equal_i(GIT_REQUEST_UPLOAD_PACK, g_server->type);
	cl_assert_equal_s("/project.git", g_server->path);
}
