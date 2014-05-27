#include "clar_libgit2.h"
#include "transports/smart.h"

static git_pkt *pkt;

void test_network_protocol_negotiation__cleanup(void)
{
	git_pkt_free(pkt);
	pkt = NULL;
}

void test_network_protocol_negotiation__have(void)
{
	const char buf[] = "0032have 7e47fe2bd8d01d481f44d7af0531bd93d3b21c01\n";
	const char *rest;
	git_oid id;
	git_pkt_have_want *ppkt;

	git_oid_fromstr(&id, "7e47fe2bd8d01d481f44d7af0531bd93d3b21c01");

	cl_git_pass(git_pkt_parse_line(&pkt, buf, &rest, sizeof(buf)));
	cl_assert_equal_i(GIT_PKT_HAVE, pkt->type);
	ppkt = (git_pkt_have_want *) pkt;
	cl_assert(!git_oid_cmp(&id, &ppkt->id));
}

void test_network_protocol_negotiation__want(void)
{
	const char buf[] = "0032want 7e47fe2bd8d01d481f44d7af0531bd93d3b21c01\n";
	const char *rest;
	git_oid id;
	git_pkt_have_want *ppkt;

	git_oid_fromstr(&id, "7e47fe2bd8d01d481f44d7af0531bd93d3b21c01");

	cl_git_pass(git_pkt_parse_line(&pkt, buf, &rest, sizeof(buf)));
	cl_assert_equal_i(GIT_PKT_WANT, pkt->type);
	ppkt = (git_pkt_have_want *) pkt;
	cl_assert(!git_oid_cmp(&id, &ppkt->id));
}
