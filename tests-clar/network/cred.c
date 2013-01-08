#include "clar_libgit2.h"

#include "git2/transport.h"

void test_network_cred__stock_userpass_validates_args(void)
{
	git_cred_stock_userpass_plaintext_payload payload = {0};

	cl_git_fail(git_cred_stock_userpass_plaintext(NULL, NULL, 0, NULL));

	payload.username = "user";
	cl_git_fail(git_cred_stock_userpass_plaintext(NULL, NULL, 0, &payload));

	payload.username = NULL;
	payload.username = "pass";
	cl_git_fail(git_cred_stock_userpass_plaintext(NULL, NULL, 0, &payload));
}

void test_network_cred__stock_userpass_validates_that_method_is_allowed(void)
{
	git_cred *cred;
	git_cred_stock_userpass_plaintext_payload payload = {"user", "pass"};

	cl_git_fail(git_cred_stock_userpass_plaintext(&cred, NULL, 0, &payload));
	cl_git_pass(git_cred_stock_userpass_plaintext(&cred, NULL, GIT_CREDTYPE_USERPASS_PLAINTEXT, &payload));
	git__free(cred);
}
