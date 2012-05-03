#include "clar_libgit2.h"
#include "buffer.h"
#include "refspec.h"
#include "transport.h"

static git_remote *_remote;
static git_repository *_repo;
static const git_refspec *_refspec;

void test_network_remotes__initialize(void)
{
	cl_fixture_sandbox("testrepo.git");

	cl_git_pass(git_repository_open(&_repo, "testrepo.git"));
	cl_git_pass(git_remote_load(&_remote, _repo, "test"));

	_refspec = git_remote_fetchspec(_remote);
	cl_assert(_refspec != NULL);
}

void test_network_remotes__cleanup(void)
{
	git_remote_free(_remote);
	git_repository_free(_repo);
	cl_fixture_cleanup("testrepo.git");
}

void test_network_remotes__parsing(void)
{
	cl_assert_equal_s(git_remote_name(_remote), "test");
	cl_assert_equal_s(git_remote_url(_remote), "git://github.com/libgit2/libgit2");
}

void test_network_remotes__parsing_ssh_remote(void)
{
	cl_assert( git_remote_valid_url("git@github.com:libgit2/libgit2.git") );
}

void test_network_remotes__parsing_local_path_fails_if_path_not_found(void)
{
	cl_assert( !git_remote_valid_url("/home/git/repos/libgit2.git") );
}

void test_network_remotes__supported_transport_methods_are_supported(void)
{
  cl_assert( git_remote_supported_url("git://github.com/libgit2/libgit2") );
}

void test_network_remotes__unsupported_transport_methods_are_unsupported(void)
{
	cl_assert( !git_remote_supported_url("git@github.com:libgit2/libgit2.git") );
}

void test_network_remotes__refspec_parsing(void)
{
	cl_assert_equal_s(git_refspec_src(_refspec), "refs/heads/*");
	cl_assert_equal_s(git_refspec_dst(_refspec), "refs/remotes/test/*");
}

void test_network_remotes__set_fetchspec(void)
{
	cl_git_pass(git_remote_set_fetchspec(_remote, "refs/*:refs/*"));
	_refspec = git_remote_fetchspec(_remote);
	cl_assert_equal_s(git_refspec_src(_refspec), "refs/*");
	cl_assert_equal_s(git_refspec_dst(_refspec), "refs/*");
}

void test_network_remotes__set_pushspec(void)
{
	cl_git_pass(git_remote_set_pushspec(_remote, "refs/*:refs/*"));
	_refspec = git_remote_pushspec(_remote);
	cl_assert_equal_s(git_refspec_src(_refspec), "refs/*");
	cl_assert_equal_s(git_refspec_dst(_refspec), "refs/*");
}

void test_network_remotes__save(void)
{
	git_remote_free(_remote);

	/* Set up the remote and save it to config */
	cl_git_pass(git_remote_new(&_remote, _repo, "upstream", "git://github.com/libgit2/libgit2", NULL));
	cl_git_pass(git_remote_set_fetchspec(_remote, "refs/heads/*:refs/remotes/upstream/*"));
	cl_git_pass(git_remote_set_pushspec(_remote, "refs/heads/*:refs/heads/*"));
	cl_git_pass(git_remote_save(_remote));
	git_remote_free(_remote);
	_remote = NULL;

	/* Load it from config and make sure everything matches */
	cl_git_pass(git_remote_load(&_remote, _repo, "upstream"));

	_refspec = git_remote_fetchspec(_remote);
	cl_assert(_refspec != NULL);
	cl_assert_equal_s(git_refspec_src(_refspec), "refs/heads/*");
	cl_assert_equal_s(git_refspec_dst(_refspec), "refs/remotes/upstream/*");

	_refspec = git_remote_pushspec(_remote);
	cl_assert(_refspec != NULL);
	cl_assert_equal_s(git_refspec_src(_refspec), "refs/heads/*");
	cl_assert_equal_s(git_refspec_dst(_refspec), "refs/heads/*");
}

void test_network_remotes__fnmatch(void)
{
	cl_assert(git_refspec_src_matches(_refspec, "refs/heads/master"));
	cl_assert(git_refspec_src_matches(_refspec, "refs/heads/multi/level/branch"));
}

void test_network_remotes__transform(void)
{
	char ref[1024];

	memset(ref, 0x0, sizeof(ref));
	cl_git_pass(git_refspec_transform(ref, sizeof(ref), _refspec, "refs/heads/master"));
	cl_assert_equal_s(ref, "refs/remotes/test/master");
}

void test_network_remotes__transform_r(void)
{
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_refspec_transform_r(&buf,  _refspec, "refs/heads/master"));
	cl_assert_equal_s(git_buf_cstr(&buf), "refs/remotes/test/master");
	git_buf_free(&buf);
}

void test_network_remotes__missing_refspecs(void)
{
	git_config *cfg;

	git_remote_free(_remote);

	cl_git_pass(git_repository_config(&cfg, _repo));
	cl_git_pass(git_config_set_string(cfg, "remote.specless.url", "http://example.com"));
	cl_git_pass(git_remote_load(&_remote, _repo, "specless"));

	git_config_free(cfg);
}

void test_network_remotes__list(void)
{
	git_strarray list;
	git_config *cfg;

	cl_git_pass(git_remote_list(&list, _repo));
	cl_assert(list.count == 1);
	git_strarray_free(&list);

	cl_git_pass(git_repository_config(&cfg, _repo));
	cl_git_pass(git_config_set_string(cfg, "remote.specless.url", "http://example.com"));
	cl_git_pass(git_remote_list(&list, _repo));
	cl_assert(list.count == 2);
	git_strarray_free(&list);

	git_config_free(cfg);
}

void test_network_remotes__loading_a_missing_remote_returns_ENOTFOUND(void)
{
	cl_assert_equal_i(GIT_ENOTFOUND, git_remote_load(&_remote, _repo, "just-left-few-minutes-ago"));
}

void test_network_remotes__add(void)
{
	git_remote_free(_remote);
	cl_git_pass(git_remote_add(&_remote, _repo, "addtest", "http://github.com/libgit2/libgit2"));
	git_remote_free(_remote);

	cl_git_pass(git_remote_load(&_remote, _repo, "addtest"));
	_refspec = git_remote_fetchspec(_remote);
	cl_assert(!strcmp(git_refspec_src(_refspec), "refs/heads/*"));
	cl_assert(!strcmp(git_refspec_dst(_refspec), "refs/remotes/addtest/*"));
}
