#include "clar_libgit2.h"
#include "futils.h"
#include "repository.h"
#include "sysdir.h"
#include <ctype.h>

static git_repository *repo;

void test_repo_extensions__initialize(void)
{
	git_config *config;

	repo = cl_git_sandbox_init("empty_bare.git");

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_int32(config, "core.repositoryformatversion", 1));
	git_config_free(config);
}

void test_repo_extensions__cleanup(void)
{
	cl_git_sandbox_cleanup();
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_EXTENSIONS, NULL, 0));
}

void test_repo_extensions__builtin(void)
{
	git_repository *extended;

	cl_repo_set_string(repo, "extensions.noop", "foobar");

	cl_git_pass(git_repository_open(&extended, "empty_bare.git"));
	cl_assert(git_repository_path(extended) != NULL);
	cl_assert(git__suffixcmp(git_repository_path(extended), "/") == 0);
	git_repository_free(extended);
}

void test_repo_extensions__negate_builtin(void)
{
	const char *in[] = { "foo", "!noop", "baz" };
	git_repository *extended;

	cl_repo_set_string(repo, "extensions.noop", "foobar");

	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_EXTENSIONS, in, ARRAY_SIZE(in)));

	cl_git_fail(git_repository_open(&extended, "empty_bare.git"));
	git_repository_free(extended);
}

void test_repo_extensions__unsupported(void)
{
	git_repository *extended = NULL;

	cl_repo_set_string(repo, "extensions.unknown", "foobar");

	cl_git_fail(git_repository_open(&extended, "empty_bare.git"));
	git_repository_free(extended);
}

void test_repo_extensions__adds_extension(void)
{
	const char *in[] = { "foo", "!noop", "newextension", "baz" };
	git_repository *extended;

	cl_repo_set_string(repo, "extensions.newextension", "foobar");
	cl_git_pass(git_libgit2_opts(GIT_OPT_SET_EXTENSIONS, in, ARRAY_SIZE(in)));

	cl_git_pass(git_repository_open(&extended, "empty_bare.git"));
	cl_assert(git_repository_path(extended) != NULL);
	cl_assert(git__suffixcmp(git_repository_path(extended), "/") == 0);
	git_repository_free(extended);
}

void test_repo_extensions__preciousobjects(void)
{
	git_repository *extended = NULL;

	cl_repo_set_string(repo, "extensions.preciousObjects", "true");

	cl_git_pass(git_repository_open(&extended, "empty_bare.git"));
	git_repository_free(extended);
}

void test_repo_extensions__relativeworktrees(void)
{
	git_repository *extended = NULL;

	cl_repo_set_string(repo, "extensions.relativeWorktrees", "true");

	cl_git_pass(git_repository_open(&extended, "empty_bare.git"));
	git_repository_free(extended);
}

void test_repo_extensions__partialclone(void)
{
	git_config *config;
	git_repository *extended;
	const char *remote_name;
	const char *filter_spec;
	int version;
	int promisor;

	cl_git_pass(git_repository__set_partial_clone(repo, "origin", "blob:none"));
	cl_git_pass(git_repository_config_snapshot(&config, repo));

	cl_git_pass(git_config_get_int32(
		&version, config, "core.repositoryformatversion"));
	cl_assert_equal_i(1, version);

	cl_git_pass(git_config_get_string(
		&remote_name, config, "extensions.partialclone"));
	cl_assert_equal_s("origin", remote_name);

	cl_git_pass(git_config_get_bool(
		&promisor, config, "remote.origin.promisor"));
	cl_assert(promisor);

	cl_git_pass(git_config_get_string(
		&filter_spec, config, "remote.origin.partialclonefilter"));
	cl_assert_equal_s("blob:none", filter_spec);

	git_config_free(config);

	cl_git_pass(git_repository_open(&extended, "empty_bare.git"));
	git_repository_free(extended);
}
