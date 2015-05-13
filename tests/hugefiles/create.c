/**
 * This test tries to create a huge file and add it.
 * The purpose is to ensure various APIs accept 64-bit
 * file sizes/offsets.
 */
#include "clar_libgit2.h"

#include "git2/clone.h"
#include "remote.h"
#include "fileops.h"
#include "repository.h"

#define FILE_A      "file_a.zeroes"
#define REPO_ROOT   "./repo1"

static git_repository *g_repo = NULL;
static git_oid g_id_initial_commit;

/**
 * Create a new, empty repo. Seed it with an initial commit on branch "master".
 */
void test_hugefiles_create__initialize(void)
{
	char *envvar = NULL;
	git_signature *sig = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_oid id_tree;

	envvar = cl_getenv("GITTEST_INVASIVE_FS_SIZE");
	if (!envvar)
		cl_skip();

#ifdef GIT_WIN32
	git__free(envvar);
#endif

	cl_git_pass(git_repository_init(&g_repo, REPO_ROOT, 0));

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_write_tree(&id_tree, index));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_tree));

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_commit_create_v(&g_id_initial_commit, g_repo, "HEAD", sig, sig, NULL, "Initial Commit", tree, 0));

	git_tree_free(tree);
	git_signature_free(sig);
	git_index_free(index);
}

void test_hugefiles_create__cleanup(void)
{
	git_repository_free(g_repo);
	cl_git_pass(git_futils_rmdir_r(REPO_ROOT, NULL, GIT_RMDIR_REMOVE_FILES));
}

/**
 * This should create a huge file of ZEROES of the requested size.
 */
static void _create_zero_file_using_ftruncate(const char *rr_filename, git_off_t i64len)
{
	git_buf buf = GIT_BUF_INIT;
	struct stat st;
	int fd = -1;
	int error;

	/* Tests don't run with the CWD just above the repo-root.
	 * Sometimes we need cwd-relative paths and sometimes we
	 * need repo-root-relative paths.
	 */
	cl_git_pass(git_buf_joinpath(&buf, REPO_ROOT, rr_filename));

	cl_must_pass((fd = p_open(buf.ptr, O_CREAT | O_RDWR, 0644)));

	assert(i64len > 0xffffffff);

	cl_assert((error = p_ftruncate(fd, i64len)) == 0);
	cl_assert((error = p_fstat(fd, &st)) == 0);
	cl_assert(st.st_size == i64len);

	p_close(fd);

	git_buf_free(&buf);
}

/**
 * Stage the given repo-relative file.
 */
static void _stage_file(const char *rr_filename)
{
	git_index *index = NULL;

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, rr_filename));
	cl_git_pass(git_index_write(index));

	git_index_free(index);
}

static void _commit_repo(const char *msg)
{
	git_signature *sig = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_commit *commit_parent = NULL;
	git_oid id_tree;
	git_oid id_commit;

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_write_tree(&id_tree, index));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_tree));

	cl_git_pass(git_commit_lookup(&commit_parent, g_repo, &g_id_initial_commit));

	cl_git_pass(git_commit_create(&id_commit, g_repo, "HEAD", sig, sig, NULL, msg, tree, 1, &commit_parent));

	git_signature_free(sig);
	git_index_free(index);
	git_tree_free(tree);
	git_commit_free(commit_parent);
}

/**
 * This test uses p_ftruncate() to create a huge file
 * and confirm that it can be staged and commited.
 * Since it is all zeros, it will compress greatly and
 * so doesn't fully stress the system, but this is here
 * to verify the APIs.
 */
void test_hugefiles_create__4g1(void)
{
#if defined(__MINGW32__) && !defined(MINGW_HAS_SECURE_API)
	/* Ming32 needs a 64-bit version of _chsize_s(). */
	cl_skip();
#endif

	_create_zero_file_using_ftruncate(FILE_A, 0x100000001);
	_stage_file(FILE_A);
	_commit_repo("4g1");
}
