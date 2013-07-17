#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/sys/index.h"
#include "fileops.h"

static git_repository *g_repo;
static git_index *g_index;

#define TEST_REPO_PATH "merge-resolve"

#define CONFLICTING_ANCESTOR_OID   "d427e0b2e138501a3d15cc376077a3631e15bd46"
#define CONFLICTING_OURS_OID       "4e886e602529caa9ab11d71f86634bd1b6e0de10"
#define CONFLICTING_THEIRS_OID     "2bd0a343aeef7a2cf0d158478966a6e587ff3863"

#define AUTOMERGEABLE_ANCESTOR_OID "6212c31dab5e482247d7977e4f0dd3601decf13b"
#define AUTOMERGEABLE_OURS_OID     "ee3fa1b8c00aff7fe02065fdb50864bb0d932ccf"
#define AUTOMERGEABLE_THEIRS_OID   "058541fc37114bfc1dddf6bd6bffc7fae5c2e6fe"

#define LINK_ANCESTOR_OID          "1a010b1c0f081b2e8901d55307a15c29ff30af0e"
#define LINK_OURS_OID              "72ea499e108df5ff0a4a913e7655bbeeb1fb69f2"
#define LINK_THEIRS_OID            "8bfb012a6d809e499bd8d3e194a3929bc8995b93"

#define LINK_ANCESTOR_TARGET       "file"
#define LINK_OURS_TARGET           "other-file"
#define LINK_THEIRS_TARGET         "still-another-file"

#define CONFLICTING_OURS_FILE \
	"this file is changed in master and branch\n"
#define CONFLICTING_THEIRS_FILE \
	"this file is changed in branch and master\n"
#define CONFLICTING_DIFF3_FILE \
	"<<<<<<< ours\n" \
	"this file is changed in master and branch\n" \
	"=======\n" \
	"this file is changed in branch and master\n" \
	">>>>>>> theirs\n"

#define AUTOMERGEABLE_MERGED_FILE \
	"this file is changed in master\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is changed in branch\n"

struct checkout_index_entry {
	uint16_t mode;
	char oid_str[41];
	int stage;
	char path[128];
};

void test_checkout_conflict__initialize(void)
{
	g_repo = cl_git_sandbox_init(TEST_REPO_PATH);
	git_repository_index(&g_index, g_repo);

	cl_git_rewritefile(
		TEST_REPO_PATH "/.gitattributes",
		"* text eol=lf\n");
}

void test_checkout_conflict__cleanup(void)
{
	git_index_free(g_index);
	cl_git_sandbox_cleanup();
}

static void create_index(struct checkout_index_entry *entries, size_t entries_len)
{
	git_buf path = GIT_BUF_INIT;
	size_t i;

	for (i = 0; i < entries_len; i++) {
		git_buf_joinpath(&path, TEST_REPO_PATH, entries[i].path);
		p_unlink(git_buf_cstr(&path));

		git_index_remove_bypath(g_index, entries[i].path);
	}

	for (i = 0; i < entries_len; i++) {
		git_index_entry entry;

		memset(&entry, 0x0, sizeof(git_index_entry));

		entry.mode = entries[i].mode;
		entry.flags = entries[i].stage << GIT_IDXENTRY_STAGESHIFT;
		git_oid_fromstr(&entry.oid, entries[i].oid_str);
		entry.path = entries[i].path;

		git_index_add(g_index, &entry);
	}

	git_buf_free(&path);
}

static void create_conflicting_index(void)
{
	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "conflicting.txt" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "conflicting.txt" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "conflicting.txt" },
	};

	create_index(checkout_index_entries, 3);
	git_index_write(g_index);
}

static void ensure_workdir_contents(const char *path, const char *contents)
{
	git_buf fullpath = GIT_BUF_INIT, data_buf = GIT_BUF_INIT;

	cl_git_pass(
		git_buf_joinpath(&fullpath, git_repository_workdir(g_repo), path));

	cl_git_pass(git_futils_readbuffer(&data_buf, git_buf_cstr(&fullpath)));
	cl_assert(strcmp(git_buf_cstr(&data_buf), contents) == 0);

	git_buf_free(&fullpath);
	git_buf_free(&data_buf);
}

static void ensure_workdir_oid(const char *path, const char *oid_str)
{
	git_oid expected, actual;

	cl_git_pass(git_oid_fromstr(&expected, oid_str));
	cl_git_pass(git_repository_hashfile(&actual, g_repo, path, GIT_OBJ_BLOB, NULL));
	cl_assert(git_oid_cmp(&expected, &actual) == 0);
}

static void ensure_workdir_mode(const char *path, int mode)
{
#ifndef GIT_WIN32
	git_buf fullpath = GIT_BUF_INIT;
	struct stat st;

	cl_git_pass(
		git_buf_joinpath(&fullpath, git_repository_workdir(g_repo), path));

	cl_git_pass(p_stat(git_buf_cstr(&fullpath), &st));
	cl_assert_equal_i(mode, st.st_mode);

	git_buf_free(&fullpath);
#endif
}

static void ensure_workdir_link(const char *path, const char *target)
{
#ifdef GIT_WIN32
	ensure_workdir_contents(path, target);
#else
	git_buf fullpath = GIT_BUF_INIT;
	char actual[1024];
	struct stat st;
	int len;

	cl_git_pass(
		git_buf_joinpath(&fullpath, git_repository_workdir(g_repo), path));

	cl_git_pass(p_lstat(git_buf_cstr(&fullpath), &st));
	cl_assert(S_ISLNK(st.st_mode));

	cl_assert((len = p_readlink(git_buf_cstr(&fullpath), actual, 1024)) > 0);
	actual[len] = '\0';
	cl_assert(strcmp(actual, target) == 0);

	git_buf_free(&fullpath);
#endif
}

void test_checkout_conflict__ignored(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	opts.checkout_strategy |= GIT_CHECKOUT_SKIP_UNMERGED;

	create_conflicting_index();

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	cl_assert(!git_path_exists(TEST_REPO_PATH "/conflicting.txt"));
}

void test_checkout_conflict__ours(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	opts.checkout_strategy |= GIT_CHECKOUT_USE_OURS;

	create_conflicting_index();

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_contents("conflicting.txt", CONFLICTING_OURS_FILE);
}

void test_checkout_conflict__theirs(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	opts.checkout_strategy |= GIT_CHECKOUT_USE_THEIRS;

	create_conflicting_index();

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_contents("conflicting.txt", CONFLICTING_THEIRS_FILE);

}

void test_checkout_conflict__diff3(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	create_conflicting_index();

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_contents("conflicting.txt", CONFLICTING_DIFF3_FILE);
}

void test_checkout_conflict__automerge(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, AUTOMERGEABLE_ANCESTOR_OID, 1, "automergeable.txt" },
		{ 0100644, AUTOMERGEABLE_OURS_OID, 2, "automergeable.txt" },
		{ 0100644, AUTOMERGEABLE_THEIRS_OID, 3, "automergeable.txt" },
	};

	create_index(checkout_index_entries, 3);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_contents("automergeable.txt", AUTOMERGEABLE_MERGED_FILE);
}

void test_checkout_conflict__directory_file(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-1" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "df-1" },
		{ 0100644, CONFLICTING_THEIRS_OID, 0, "df-1/file" },

		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-2" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-2" },
		{ 0100644, CONFLICTING_OURS_OID, 0, "df-2/file" },

		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-3" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-3/file" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "df-3/file" },

		{ 0100644, CONFLICTING_OURS_OID, 2, "df-4" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-4/file" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-4/file" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	create_index(checkout_index_entries, 12);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_oid("df-1/file", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-1~ours", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-2/file", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-2~theirs", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-3/file", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-3~theirs", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-4~ours", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-4/file", CONFLICTING_THEIRS_OID);
}

void test_checkout_conflict__directory_file_with_custom_labels(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-1" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "df-1" },
		{ 0100644, CONFLICTING_THEIRS_OID, 0, "df-1/file" },

		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-2" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-2" },
		{ 0100644, CONFLICTING_OURS_OID, 0, "df-2/file" },

		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-3" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-3/file" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "df-3/file" },

		{ 0100644, CONFLICTING_OURS_OID, 2, "df-4" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "df-4/file" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "df-4/file" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;
	opts.our_label = "HEAD";
	opts.their_label = "branch";

	create_index(checkout_index_entries, 12);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	ensure_workdir_oid("df-1/file", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-1~HEAD", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-2/file", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-2~branch", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-3/file", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-3~branch", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("df-4~HEAD", CONFLICTING_OURS_OID);
	ensure_workdir_oid("df-4/file", CONFLICTING_THEIRS_OID);
}

void test_checkout_conflict__link_file(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "link-1" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "link-1" },
		{ 0120000, LINK_THEIRS_OID, 3, "link-1" },

		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "link-2" },
		{ 0120000, LINK_OURS_OID, 2, "link-2" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "link-2" },

		{ 0120000, LINK_ANCESTOR_OID, 1, "link-3" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "link-3" },
		{ 0120000, LINK_THEIRS_OID, 3, "link-3" },

		{ 0120000, LINK_ANCESTOR_OID, 1, "link-4" },
		{ 0120000, LINK_OURS_OID, 2, "link-4" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "link-4" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	create_index(checkout_index_entries, 12);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	/* Typechange conflicts always keep the file in the workdir */
	ensure_workdir_oid("link-1", CONFLICTING_OURS_OID);
	ensure_workdir_oid("link-2", CONFLICTING_THEIRS_OID);
	ensure_workdir_oid("link-3", CONFLICTING_OURS_OID);
	ensure_workdir_oid("link-4", CONFLICTING_THEIRS_OID);
}

void test_checkout_conflict__links(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0120000, LINK_ANCESTOR_OID, 1, "link-1" },
		{ 0120000, LINK_OURS_OID, 2, "link-1" },
		{ 0120000, LINK_THEIRS_OID, 3, "link-1" },

		{ 0120000, LINK_OURS_OID, 2, "link-2" },
		{ 0120000, LINK_THEIRS_OID, 3, "link-2" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	create_index(checkout_index_entries, 5);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	/* Conflicts with links always keep the ours side (even with -Xtheirs) */
	ensure_workdir_link("link-1", LINK_OURS_TARGET);
	ensure_workdir_link("link-2", LINK_OURS_TARGET);
}

void test_checkout_conflict__add_add(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_OURS_OID, 2, "conflicting.txt" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "conflicting.txt" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	create_index(checkout_index_entries, 2);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	/* Add/add writes diff3 files */
	ensure_workdir_contents("conflicting.txt", CONFLICTING_DIFF3_FILE);
}

void test_checkout_conflict__mode_change(void)
{
	git_checkout_opts opts = GIT_CHECKOUT_OPTS_INIT;

	struct checkout_index_entry checkout_index_entries[] = {
		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "executable-1" },
		{ 0100755, CONFLICTING_ANCESTOR_OID, 2, "executable-1" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "executable-1" },

		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "executable-2" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "executable-2" },
		{ 0100755, CONFLICTING_ANCESTOR_OID, 3, "executable-2" },

		{ 0100755, CONFLICTING_ANCESTOR_OID, 1, "executable-3" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 2, "executable-3" },
		{ 0100755, CONFLICTING_THEIRS_OID, 3, "executable-3" },

		{ 0100755, CONFLICTING_ANCESTOR_OID, 1, "executable-4" },
		{ 0100755, CONFLICTING_OURS_OID, 2, "executable-4" },
		{ 0100644, CONFLICTING_ANCESTOR_OID, 3, "executable-4" },

		{ 0100644, CONFLICTING_ANCESTOR_OID, 1, "executable-5" },
		{ 0100755, CONFLICTING_OURS_OID, 2, "executable-5" },
		{ 0100644, CONFLICTING_THEIRS_OID, 3, "executable-5" },

		{ 0100755, CONFLICTING_ANCESTOR_OID, 1, "executable-6" },
		{ 0100644, CONFLICTING_OURS_OID, 2, "executable-6" },
		{ 0100755, CONFLICTING_THEIRS_OID, 3, "executable-6" },
	};

	opts.checkout_strategy |= GIT_CHECKOUT_SAFE;

	create_index(checkout_index_entries, 18);
	git_index_write(g_index);

	cl_git_pass(git_checkout_index(g_repo, g_index, &opts));

	/* Keep the modified mode */
	ensure_workdir_oid("executable-1", CONFLICTING_THEIRS_OID);
	ensure_workdir_mode("executable-1", 0100755);

	ensure_workdir_oid("executable-2", CONFLICTING_OURS_OID);
	ensure_workdir_mode("executable-2", 0100755);

	ensure_workdir_oid("executable-3", CONFLICTING_THEIRS_OID);
	ensure_workdir_mode("executable-3", 0100644);

	ensure_workdir_oid("executable-4", CONFLICTING_OURS_OID);
	ensure_workdir_mode("executable-4", 0100644);

	ensure_workdir_contents("executable-5", CONFLICTING_DIFF3_FILE);
	ensure_workdir_mode("executable-5", 0100755);

	ensure_workdir_contents("executable-6", CONFLICTING_DIFF3_FILE);
	ensure_workdir_mode("executable-6", 0100644);
}
