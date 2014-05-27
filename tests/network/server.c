#include "clar_libgit2.h"
#include "transports/smart.h"
#include "server.h"

static git_server *g_server;
static git_pkt *g_pkt;
static git_repository *g_repo;

void test_network_server__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
}

void test_network_server__cleanup(void)
{
	git_pkt_free(g_pkt);
	git_server_free(g_server);
	git_repository_free(g_repo);
}

void test_network_server__request_type(void)
{
	const char buf[] = "0032git-upload-pack /project.git\0host=myserver.com\0";
	const char *rest;
	size_t buflen = sizeof(buf);

	cl_git_pass(git_server_new(&g_server, g_repo, 0));
	cl_git_pass(git_pkt_parse_line(&g_pkt, buf, &rest, buflen));
	cl_git_pass(git_server__handle_request(g_server, g_pkt));

	cl_assert_equal_i(GIT_REQUEST_UPLOAD_PACK, g_server->type);
	cl_assert_equal_s("/project.git", g_server->path);
}

const char *exptected_ls = "0032a65fedf39aefe402d3bb6e24df4d4f5fe4547750 HEAD\n"
	"003ca4a7dce85cf63874e984719f4fdd239f5145052f refs/heads/br2\n"
	"0045a4a7dce85cf63874e984719f4fdd239f5145052f refs/heads/cannot-fetch\n"
	"0040e90810b8df3e80c413d903f631643c716887138d refs/heads/chomped\n"
	"0040258f0e2a959a364e40ed6603d5d44fbb24765b10 refs/heads/haacked\n"
	"003fa65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/heads/master\n"
	"0041a65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/heads/not-good\n"
	"003f41bc8c69075bbdb46c5c6f0566cc8cc5b46e8bd9 refs/heads/packed\n"
	"00444a202b346bb0fb0db7eff3cffeb3c70babbd2045 refs/heads/packed-test\n"
	"0041763d71aadf09a7951596c9746c024e7eece7c7af refs/heads/subtrees\n"
	"003de90810b8df3e80c413d903f631643c716887138d refs/heads/test\n"
	"00449fd738e8f7967c078dceed8190330fc8648ee56a refs/heads/track-local\n"
	"0041e90810b8df3e80c413d903f631643c716887138d refs/heads/trailing\n"
	"003fd07b0f9a8c89f1d9e74dc4fce6421dec5ef8a659 refs/notes/fanout\n"
	"0046be3563ae3f795b2b4353bcce3a527ad0a4f7f644 refs/remotes/test/master\n"
	"004d521d87c1ec3aef9824daf6d96cc0ae3710766d91 refs/tags/annotated_tag_to_blob\n"
	"00501385f264afb75a56a5bec74243be9b367ba4ca08 refs/tags/annotated_tag_to_blob^{}\n"
	"003f7b4384978d2493e851f9cca7858815fac9b10980 refs/tags/e90810b\n"
	"0042e90810b8df3e80c413d903f631643c716887138d refs/tags/e90810b^{}\n"
	"0040849a5e34a26815e821f865b8479f5815a47af0fe refs/tags/hard_tag\n"
	"0043a65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/tags/hard_tag^{}\n"
	"00451385f264afb75a56a5bec74243be9b367ba4ca08 refs/tags/point_to_blob\n"
	"00424a23e2e65ad4e31c4c9db7dc746650bfad082679 refs/tags/taggerless\n"
	"0045e90810b8df3e80c413d903f631643c716887138d refs/tags/taggerless^{}\n"
	"003cb25fa35b38051e4ae45d4222e795f9df2e43f1d1 refs/tags/test\n"
	"003fe90810b8df3e80c413d903f631643c716887138d refs/tags/test^{}\n"
	"0043849a5e34a26815e821f865b8479f5815a47af0fe refs/tags/wrapped_tag\n"
	"0046a65fedf39aefe402d3bb6e24df4d4f5fe4547750 refs/tags/wrapped_tag^{}\n"
	"0000";

void test_network_server__upload_pack_ls(void)
{
	const char buf[] = "0032git-upload-pack /project.git\0host=myserver.com\0";
	const char *rest;
	size_t buflen = sizeof(buf);
	git_buf listing = GIT_BUF_INIT;

	cl_git_pass(git_server_new(&g_server, g_repo, 0));
	cl_git_pass(git_pkt_parse_line(&g_pkt, buf, &rest, buflen));
	cl_git_pass(git_server__handle_request(g_server, g_pkt));

	cl_assert_equal_i(GIT_REQUEST_UPLOAD_PACK, g_server->type);
	cl_assert_equal_s("/project.git", g_server->path);

	cl_git_pass(git_server__ls(&listing, g_server));
	cl_assert_equal_s(exptected_ls, git_buf_cstr(&listing));

	git_buf_free(&listing);
}
