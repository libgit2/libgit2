#include "clar_libgit2.h"
#include "git2/sys/repository.h"
#include "repository.h"
#include "odb.h"

void test_repo_extensions__initialize(void)
{
	git_repository *repo;
	git_config *config;

	repo = cl_git_sandbox_init("empty_bare.git");

	cl_git_pass(git_repository_config(&config, repo));
	cl_git_pass(git_config_set_int32(config, "core.repositoryformatversion", 1));
	cl_git_pass(git_config_set_string(config, "extensions.noop", "true"));
	cl_git_pass(git_config_set_string(config, "extensions.foo", "false"));

	git_config_free(config);
	git_repository_free(repo);
}

void test_repo_extensions__finalize(void)
{
}

static int accept_foo_cb_called;
int accept_foo_cb(git_repository *repo, const git_config_entry *entry)
{
	GIT_UNUSED(repo);

	accept_foo_cb_called++;
	cl_assert_equal_s("extensions.foo", entry->name);
	cl_assert_equal_s("false", entry->value);


	return 0;
}

void test_repo_extensions__callback(void)
{
	git_repository *repo;

	accept_foo_cb_called = 0;
	cl_git_pass(git_repository_open_ext(&repo, "empty_bare.git", 0, NULL, accept_foo_cb));
	git_repository_free(repo);

	/* 'noop' and 'foo' */
	cl_assert_equal_i(1, accept_foo_cb_called);
}

static git_odb *g_odb;
int own_odb_cb(git_repository *repo, const git_config_entry *entry)
{
	git_buf odb_path = GIT_BUF_INIT;

	if (strcmp("extensions.foo", entry->name) || strcmp("false", entry->value))
		return GIT_PASSTHROUGH;

	cl_git_pass(git_buf_joinpath(&odb_path, git_repository_path(repo), GIT_OBJECTS_DIR));
	cl_git_pass(git_odb_open(&g_odb, odb_path.ptr));
	git_repository_set_odb(repo, g_odb);

	git_buf_free(&odb_path);
	return 0;
}

void test_repo_extensions__own_odb(void)
{
	git_repository *repo;
	git_odb *odb;

	g_odb = NULL;
	cl_git_pass(git_repository_open_ext(&repo, "empty_bare.git", 0, NULL, own_odb_cb));

	cl_git_pass(git_repository_odb__weakptr(&odb, repo));
	cl_assert_equal_p(g_odb, odb);

	git_repository_free(repo);
}
