/**
 * This test creates a big repo (larger than 4GB) and tries to clone it.
 * The purpose is to confirm that we can build and receive large packfiles.
 */
#include "clar_libgit2.h"

#include "git2/clone.h"
#include "remote.h"
#include "fileops.h"
#include "repository.h"
#include "oid.h"
#include "hash.h"

#define CREATE_REPO_ROOT   "./repo_src"

static git_repository *g_repo = NULL;
static git_oid g_id_last_commit;

static bool is_invasive(void)
{
	char *envvar = cl_getenv("GITTEST_INVASIVE_FS_SIZE");
	bool b = (envvar != NULL);
#ifdef GIT_WIN32
	git__free(envvar);
#endif
	return b;
}

/**
 * Stage the given repo-relative file.
 */
static void stage_file(const char *rr_filename)
{
	git_index *index = NULL;

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_add_bypath(index, rr_filename));
	cl_git_pass(git_index_write(index));

	git_index_free(index);
}

static void create_attributes(void)
{
	git_buf buf = GIT_BUF_INIT;

	/* This is to make sure that we disable CRLF stuff on our random files. */
	cl_git_pass(git_buf_joinpath(&buf, CREATE_REPO_ROOT, ".gitattributes"));
	cl_git_mkfile(buf.ptr, "*.binary binary\n");
	git_buf_free(&buf);

	stage_file(".gitattributes");
}

static void commit_repo(const char *msg)
{
	git_signature *sig = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_commit *commit_parent = NULL;
	git_oid id_tree;

	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_write_tree(&id_tree, index));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_tree));

	cl_git_pass(git_commit_lookup(&commit_parent, g_repo, &g_id_last_commit));

	cl_git_pass(git_commit_create(&g_id_last_commit, g_repo, "HEAD", sig, sig, NULL, msg, tree, 1, &commit_parent));

	git_signature_free(sig);
	git_index_free(index);
	git_tree_free(tree);
	git_commit_free(commit_parent);
}

/**
 * Write 1GB files files with random data (so they won't compress).
 * We want the overall size of the repo to be big.
 */
static void write_random_data_1gb(int fd)
{
#define MY_GOAL    (1024 * 1024 * 1024)
#define MY_U_SIZE  (64 * 1024)

	union u {
		unsigned char ach[MY_U_SIZE];
		git_oid       aid[MY_U_SIZE / sizeof(git_oid)];
		uint64_t      aui[MY_U_SIZE / sizeof(uint64_t)];
	};
	union u u;

	int nr_ach = sizeof(u.ach) / sizeof(u.ach[0]);
	int nr_aid = sizeof(u.aid) / sizeof(u.aid[0]);
	int nr_aui = sizeof(u.aui) / sizeof(u.aui[0]);
	int iters = (MY_GOAL / MY_U_SIZE);
	int k, mask;

	memset(&u, 0, sizeof(u));

	/* Fill u.aid[] with random data. We rely on SHA1(SHA1(...(SHA1(x))))
	 * to generate a series of random values and pack them into a 64kb
	 * buffer.
	 */
	git_oid_cpy(&u.aid[0], &g_id_last_commit);
	for (k=1; k < nr_aid; k++) {
		git_hash_buf(&u.aid[k], &u.aid[k-1], sizeof(u.aid[0]));
	}

	/* Treat the u.aid[] as a raw 64kb buffer and write it to the file. */
	cl_must_pass(p_write(fd, u.ach, nr_ach));

	/* Generate a series of "variations" using the 64kb buffer and write
	 * them until the file is 1gb.  We need variations so that there won't
	 * be repeated runs because we want to prevent compression.  Here we
	 * play some bit-flipping games on each uint64 within the 64kb buffer.
	 * This guarantees that any repeated runs will be 7 bytes or less.
	 */
	for (mask=1; mask<iters; mask++) {
		union u u1;

		for(k=0; k<nr_aui; k++) {
			u1.aui[k] = u.aui[k] ^ mask;
		}
		cl_must_pass(p_write(fd, u1.ach, nr_ach));
	}
}

/**
 * Create some ballast.  Write a 1gb file into the WD.
 */
static void create_ballast_1gb(const char *filename)
{
	static int count = 0;

	git_buf buf = GIT_BUF_INIT;
	int fd = -1;
	char msg[100];

	cl_git_pass(git_buf_joinpath(&buf, CREATE_REPO_ROOT, filename));
	cl_must_pass((fd = p_open(buf.ptr, O_CREAT | O_RDWR, 0644)));
	git_buf_free(&buf);

	write_random_data_1gb(fd);
	p_close(fd);

	stage_file(filename);

	sprintf(msg, "Message %05d", count++);
	commit_repo(msg);
}

static void do_clone(const char *url_clone_from, const char *new_repo_path, git_clone_local_t clone_how)
{
	git_repository *new_repo = NULL;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

	checkout_opts.checkout_strategy = GIT_CHECKOUT_NONE;

	clone_opts.checkout_opts = checkout_opts;
	clone_opts.bare = true;
	clone_opts.local = clone_how;

	cl_git_pass(git_clone(&new_repo, url_clone_from, new_repo_path, &clone_opts));

	git_repository_free(new_repo);

	/* Since the source repo is 5+GB, each clone might also be large
	 * (depending on the args to clone).  So go ahead and delete the
	 * clone now before attempting the next clone.
	 */
	git_futils_rmdir_r(new_repo_path, NULL, GIT_RMDIR_REMOVE_FILES);
}


/**
 * Create a new, empty repo. Seed it with an initial commit on branch "master".
 */
void test_clone_big__initialize(void)
{
	git_signature *sig = NULL;
	git_index *index = NULL;
	git_tree *tree = NULL;
	git_oid id_tree;

	if (!is_invasive())
		cl_skip();

	cl_git_pass(git_repository_init(&g_repo, CREATE_REPO_ROOT, 0));

	create_attributes();

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_git_pass(git_index_write_tree(&id_tree, index));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &id_tree));
	cl_git_pass(git_signature_now(&sig, "me", "foo@example.com"));

	cl_git_pass(git_commit_create_v(&g_id_last_commit, g_repo, "HEAD", sig, sig, NULL, "Initial Commit", tree, 0));

	git_tree_free(tree);
	git_signature_free(sig);
	git_index_free(index);
}

void test_clone_big__cleanup(void)
{
	git_repository_free(g_repo);
}

void test_clone_big__one(void)
{
	int f_count, f;
	char name[30];

	/* we want the entire repo to be at least 4GB, so create 5 1GB files. */
	f_count = 5;
	for (f = 0; f < f_count; f++) {
		sprintf(name, "file%d.binary", f);
		create_ballast_1gb(name);
	}

	do_clone(CREATE_REPO_ROOT, "./repo_clone__local", GIT_CLONE_LOCAL);
	do_clone(CREATE_REPO_ROOT, "./repo_clone__no_local", GIT_CLONE_NO_LOCAL);
	do_clone(CREATE_REPO_ROOT, "./repo_clone__local_no_links", GIT_CLONE_LOCAL_NO_LINKS);
}
