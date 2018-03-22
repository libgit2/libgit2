#include "clar_libgit2.h"

#include "git2/clone.h"
#include "remote.h"
#include "fileops.h"
#include "repository.h"
#include "oid.h"
#include "hash.h"

#define BIGREPO_FIXTURE "bigrepo"
#define BIG_FILE_SIZE    (1024 * 1024 * 1024)
#define BIG_FILE_BATCH   (64 * 1024)

static git_repository *g_repo;

static void commit_file(git_repository *repo, const char *file)
{
	git_commit *parents[1] = { NULL };
	git_signature *sig = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_oid treeid, parentid;
	int nparents = 0;

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_add_bypath(index, file));
	cl_git_pass(git_index_write(index));
	cl_git_pass(git_index_write_tree(&treeid, index));
	cl_git_pass(git_tree_lookup(&tree, repo, &treeid));

	if (git_reference_name_to_id(&parentid, repo, "HEAD") == 0) {
		cl_git_pass(git_commit_lookup(&parents[0], repo, &parentid));
		nparents = 1;
	}

	cl_git_pass(git_commit_create(&parentid, repo, "HEAD", sig, sig, NULL, file, tree, nparents, (const git_commit **) parents));

	git_signature_free(sig);
	git_index_free(index);
	git_tree_free(tree);
	git_commit_free(parents[0]);
}

static void commit_attributes(git_repository *repo)
{
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_repository_item_path(&buf, repo, GIT_REPOSITORY_ITEM_WORKDIR));
	cl_git_pass(git_buf_joinpath(&buf, buf.ptr, ".gitattributes"));
	cl_git_mkfile(buf.ptr, "*.binary binary\n*.crlf crlf\n");
	git_buf_free(&buf);

	commit_file(repo, ".gitattributes");
}

static void commit_1gb_file(git_repository *repo, const char *file)
{
	uint32_t data[BIG_FILE_BATCH / sizeof(uint32_t)];
	git_buf buf = GIT_BUF_INIT;
	git_oid hash;
	size_t i, j;
	int fd;

	cl_git_pass(git_repository_item_path(&buf, repo, GIT_REPOSITORY_ITEM_WORKDIR));
	cl_git_pass(git_buf_joinpath(&buf, buf.ptr, file));
	cl_must_pass((fd = p_open(buf.ptr, O_CREAT | O_RDWR, 0644)));

	/* Use a deterministic but pseudo-random stream of data */
	cl_git_pass(git_hash_buf(&hash, file, strlen(file)));
	srand(hash.id[0]);

	for (i = 0; i < (BIG_FILE_SIZE / sizeof(data)); i++) {
		for (j = 0; j < ARRAY_SIZE(data); j++)
			data[j] = rand();
		cl_must_pass(p_write(fd, data, sizeof(data)));
	}

	cl_must_pass(p_close(fd));
	commit_file(repo, file);
	cl_must_pass(unlink(buf.ptr));

	git_buf_free(&buf);
}

static void do_clone(const git_repository *repo, const char *clone_path, git_clone_local_t clone_how)
{
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_repository *cloned = NULL;
	git_buf buf = GIT_BUF_INIT;

	cl_git_pass(git_repository_item_path(&buf, repo, GIT_REPOSITORY_ITEM_GITDIR));

	checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;
	clone_opts.checkout_opts = checkout_opts;
	clone_opts.bare = true;
	clone_opts.local = clone_how;

	cl_git_pass(git_clone(&cloned, buf.ptr, clone_path, &clone_opts));

	git_buf_free(&buf);
	git_repository_free(cloned);
	git_futils_rmdir_r(clone_path, NULL, GIT_RMDIR_REMOVE_FILES);
}

void test_clone_big__initialize(void)
{
	if (!cl_is_env_set("GITTEST_INVASIVE_FS_SIZE"))
		cl_skip();

	cl_git_pass(git_repository_init(&g_repo, BIGREPO_FIXTURE, 0));

	commit_attributes(g_repo);
}

void test_clone_big__cleanup(void)
{
	git_repository_free(g_repo);
	g_repo = NULL;
	cl_fixture_cleanup(BIGREPO_FIXTURE);
}

void test_clone_big__local_clone_succeeds(void)
{
	char name[30];
	int i;

	for (i = 0; i < 5; i++) {
		sprintf(name, "file%d.binary", i);
		commit_1gb_file(g_repo, name);
	}

	do_clone(g_repo, "clone_local", GIT_CLONE_LOCAL);
}

void test_clone_big__no_local_clone_succeeds(void)
{
	char name[30];
	int i;

	for (i = 0; i < 5; i++) {
		sprintf(name, "file%d.binary", i);
		commit_1gb_file(g_repo, name);
	}

	do_clone(g_repo, "clone_no_local", GIT_CLONE_NO_LOCAL);
}

void test_clone_big__local_clone_without_links_succeeds(void)
{
	char name[30];
	int i;

	for (i = 0; i < 5; i++) {
		sprintf(name, "file%d.binary", i);
		commit_1gb_file(g_repo, name);
	}

	do_clone(g_repo, "clone_local_no_links", GIT_CLONE_LOCAL_NO_LINKS);
}

void test_clone_big__clone_with_crlf_succeeds(void)
{
	commit_1gb_file(g_repo, "file.crlf");
	do_clone(g_repo, "clone_local_no_links", GIT_CLONE_LOCAL);
}
