#include "clar_libgit2.h"
#include "transports/smart.h"

static git_pkt *pkt;

void test_network_protocol_request__cleanup(void)
{
	git_pkt_free(pkt);
	pkt = NULL;
}

void test_network_protocol_request__upload_pack(void)
{
	const char buf[] = "0032git-upload-pack /project.git\0host=myserver.com\0";
	const char *rest;
	size_t buflen = sizeof(buf);
	git_pkt_request *pkt_req;

	cl_git_pass(git_pkt_parse_line(&pkt, buf, &rest, buflen));
	cl_assert_equal_i(GIT_PKT_REQUEST, pkt->type);

	pkt_req = (git_pkt_request *) pkt;
	cl_assert_equal_i(GIT_REQUEST_UPLOAD_PACK, pkt_req->request);
	cl_assert_equal_s("/project.git", pkt_req->path);
}

void test_network_protocol_request__receive_pack(void)
{
	const char buf[] = "0033git-receive-pack /project.git\0host=myserver.com\0";
	const char *rest;
	size_t buflen = sizeof(buf);
	git_pkt_request *pkt_req;

	cl_git_pass(git_pkt_parse_line(&pkt, buf, &rest, buflen));
	cl_assert_equal_i(GIT_PKT_REQUEST, pkt->type);

	pkt_req = (git_pkt_request *) pkt;
	cl_assert_equal_i(GIT_REQUEST_RECEIVE_PACK, pkt_req->request);
	cl_assert_equal_s("/project.git", pkt_req->path);
}

void test_network_protocol_request__upload_pack_no_nul(void)
{
	const char buf[] = "0032git-upload-pack /project.gitAhost=myserver.comA";
	const char *rest;
	size_t buflen = sizeof(buf);

	cl_git_fail_with(-1, git_pkt_parse_line(&pkt, buf, &rest, buflen));
	cl_assert_equal_s("invalid request - no terminator", giterr_last()->message);
}
