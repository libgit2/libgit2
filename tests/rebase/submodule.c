#include "clar_libgit2.h"
#include "git2/checkout.h"
#include "git2/rebase.h"
#include "posix.h"
#include "signature.h"

#include <fcntl.h>

static git_repository *repo;
static git_signature *signature;

// Fixture setup and teardown
void test_rebase_submodule__initialize(void)
{
	repo = cl_git_sandbox_init("rebase-submodule");
	cl_git_pass(git_signature_new(&signature,
		"Rebaser", "rebaser@rebaser.rb", 1405694510, 0));
}

void test_rebase_submodule__cleanup(void)
{
	git_signature_free(signature);
	cl_git_sandbox_cleanup();
}

void test_rebase_submodule__init_untracked(void)
{
	git_rebase *rebase;
	git_reference *branch_ref, *upstream_ref;
	git_annotated_commit *branch_head, *upstream_head;
	git_buf untracked_path = GIT_BUF_INIT;
	FILE *fp;
	git_submodule *submodule;
	git_config *config;

	cl_git_pass(git_reference_lookup(&branch_ref, repo, "refs/heads/asparagus"));
	cl_git_pass(git_reference_lookup(&upstream_ref, repo, "refs/heads/master"));

	cl_git_pass(git_annotated_commit_from_ref(&branch_head, repo, branch_ref));
	cl_git_pass(git_annotated_commit_from_ref(&upstream_head, repo, upstream_ref));

	git_repository_config(&config, repo);

	cl_git_pass(git_config_set_string(config, "submodule.my-submodule.url", git_repository_path(repo)));

	git_config_free(config);

	cl_git_pass(git_submodule_lookup(&submodule, repo, "my-submodule"));
	cl_git_pass(git_submodule_update(submodule, 1, NULL));

	git_buf_printf(&untracked_path, "%s/my-submodule/untracked", git_repository_workdir(repo));
	fp = fopen(git_buf_cstr(&untracked_path), "w");
	fprintf(fp, "An untracked file in a submodule should not block a rebase\n");
	fclose(fp);
	git_buf_free(&untracked_path);

	cl_git_pass(git_rebase_init(&rebase, repo, branch_head, upstream_head, NULL, NULL));

	git_submodule_free(submodule);
	git_annotated_commit_free(branch_head);
	git_annotated_commit_free(upstream_head);
	git_reference_free(branch_ref);
	git_reference_free(upstream_ref);
	git_rebase_free(rebase);
}
