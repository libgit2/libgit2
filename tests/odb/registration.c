#include "clar_libgit2.h"
#include "odb.h"
#include "backends.h"
#include "repository.h"
#include "git2/config.h"
#include "git2/sys/odb_backend.h"

static int payload_target;
static int ctor_called;
static git_odb *ctor_odb;

int odb_ctor(git_odb **out, git_repository *repo, void *payload)
{
	git_buf odb_path = GIT_BUF_INIT;

	GIT_UNUSED(payload);

	ctor_called = 1;
	cl_git_pass(git_buf_joinpath(&odb_path, git_repository_path(repo), GIT_OBJECTS_DIR));
	cl_git_pass(git_odb_open(out, odb_path.ptr));
	ctor_odb = *out;

	git_buf_free(&odb_path);
	return 0;
}

void test_odb_registration__register(void)
{
	git_odb_registration *reg;

	cl_git_pass(git_odb_register("foo", odb_ctor, &payload_target));
	reg = git_odb_registration__find("foo");

	cl_assert(reg);
	cl_assert_equal_s("foo", reg->name);
	cl_assert_equal_p(odb_ctor, reg->ctor);
	cl_assert_equal_p(&payload_target, reg->payload);

	reg = git_odb_registration__find("bar");
	cl_assert_equal_p(NULL, reg);
}

void test_odb_registration__use(void)
{
	git_config *cfg;
	git_odb *odb;
	git_repository *repo;

	cl_git_pass(git_odb_register("foo", odb_ctor, &payload_target));

	cl_git_pass(git_repository_init(&repo, "./v1-odb.git", true));
	cl_git_pass(git_repository_config__weakptr(&cfg, repo));

	cl_git_pass(git_config_set_int32(cfg, "core.repositoryformatversion", 1));
	cl_git_pass(git_config_set_string(cfg, "extensions.odb", "foo"));

	cfg = NULL;
	git_repository_free(repo);

	ctor_called = 0;
	cl_git_pass(git_repository_open(&repo, "./v1-odb.git"));
	cl_assert_equal_i(1, ctor_called);
	cl_git_pass(git_repository_odb__weakptr(&odb, repo));
	cl_assert_equal_p(ctor_odb, odb);

	git_repository_free(repo);
}
