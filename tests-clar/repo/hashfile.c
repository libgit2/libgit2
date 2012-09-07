#include "clar_libgit2.h"
#include "buffer.h"

static git_repository *_repo;

void test_repo_hashfile__initialize(void)
{
	_repo = cl_git_sandbox_init("status");
}

void test_repo_hashfile__cleanup(void)
{
	cl_git_sandbox_cleanup();
	_repo = NULL;
}

void test_repo_hashfile__simple(void)
{
	git_oid a, b;
	git_buf full = GIT_BUF_INIT;

	cl_git_pass(git_odb_hashfile(&a, "status/current_file", GIT_OBJ_BLOB));
	cl_git_pass(git_repository_hashfile(&b, _repo, "current_file", GIT_OBJ_BLOB, NULL));
	cl_assert(git_oid_equal(&a, &b));

	cl_git_pass(git_buf_joinpath(&full, git_repository_workdir(_repo), "current_file"));

	cl_git_pass(git_odb_hashfile(&a, full.ptr, GIT_OBJ_BLOB));
	cl_git_pass(git_repository_hashfile(&b, _repo, full.ptr, GIT_OBJ_BLOB, NULL));
	cl_assert(git_oid_equal(&a, &b));

	git_buf_free(&full);
}

void test_repo_hashfile__filtered(void)
{
	git_oid a, b;
	git_config *config;

	cl_git_pass(git_repository_config(&config, _repo));
	cl_git_pass(git_config_set_bool(config, "core.autocrlf", true));
	git_config_free(config);

	cl_git_append2file("status/.gitattributes", "*.txt text\n*.bin binary\n\n");

	cl_git_mkfile("status/testfile.txt", "content\r\n"); /* Content with CRLF */

	cl_git_pass(git_odb_hashfile(&a, "status/testfile.txt", GIT_OBJ_BLOB));
	cl_git_pass(git_repository_hashfile(&b, _repo, "testfile.txt", GIT_OBJ_BLOB, NULL));
	cl_assert(git_oid_cmp(&a, &b)); /* not equal */

	cl_git_pass(git_odb_hashfile(&a, "status/testfile.txt", GIT_OBJ_BLOB));
	cl_git_pass(git_repository_hashfile(&b, _repo, "testfile.txt", GIT_OBJ_BLOB, "testfile.bin"));
	cl_assert(git_oid_equal(&a, &b)); /* equal when 'binary' 'as_file' name is used */
}
