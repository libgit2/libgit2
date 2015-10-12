#include "clar_libgit2.h"
#include "backends.h"
#include "repository.h"
#include "git2/config.h"
#include "git2/sys/odb_backend.h"

int payload_target;
int ctor_called;

int odb_ctor(git_odb_backend **out, void *payload)
{
	GIT_UNUSED(payload);

	*out = NULL;
	ctor_called = 1;

	return 0;
}

void test_odb_registration__register(void)
{
	git_odb_registration *reg;

	cl_git_pass(git_odb_backend_register("foo", odb_ctor, &payload_target));
	reg = git_odb_backend__find("foo");

	cl_assert(reg);
	cl_assert_equal_s("foo", reg->name);
	cl_assert_equal_p(odb_ctor, reg->ctor);
	cl_assert_equal_p(&payload_target, reg->payload);

	reg = git_odb_backend__find("bar");
	cl_assert_equal_p(NULL, reg);
}

void test_odb_registration__use(void)
{
	git_config *cfg;
	git_repository *repo;

	cl_git_pass(git_odb_backend_register("foo", odb_ctor, &payload_target));

	cl_git_pass(git_repository_init(&repo, "./v1-odb.git", true));
	cl_git_pass(git_repository_config__weakptr(&cfg, repo));

	cl_git_pass(git_config_set_int32(cfg, "core.repositoryformatversion", 1));
	cl_git_pass(git_config_set_string(cfg, "extensions.odbbackend", "foo"));

	cfg = NULL;
	git_repository_free(repo);

	ctor_called = 0;
	cl_git_pass(git_repository_open(&repo, "./v1-odb.git"));
	cl_assert_equal_i(1, ctor_called);
}
