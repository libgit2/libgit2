#include "clar_libgit2.h"
#include "git2/repository.h"
#include "git2/merge.h"
#include "merge.h"
#include "refs.h"
#include "fileops.h"

static git_repository *repo;
static git_index *repo_index;

#define TEST_REPO_PATH "testrepo"
#define TEST_INDEX_PATH TEST_REPO_PATH "/.git/index"

#define ORIG_HEAD                   "bd593285fc7fe4ca18ccdbabf027f5d689101452"

#define THEIRS_SIMPLE_BRANCH        "branch"
#define THEIRS_SIMPLE_OID           "7cb63eed597130ba4abb87b3e544b85021905520"

#define OCTO1_BRANCH                "octo1"
#define OCTO1_OID                   "16f825815cfd20a07a75c71554e82d8eede0b061"

#define OCTO2_BRANCH                "octo2"
#define OCTO2_OID                   "158dc7bedb202f5b26502bf3574faa7f4238d56c"

#define OCTO3_BRANCH                "octo3"
#define OCTO3_OID                   "50ce7d7d01217679e26c55939eef119e0c93e272"

#define OCTO4_BRANCH                "octo4"
#define OCTO4_OID                   "54269b3f6ec3d7d4ede24dd350dd5d605495c3ae"

#define OCTO5_BRANCH                "octo5"
#define OCTO5_OID                   "e4f618a2c3ed0669308735727df5ebf2447f022f"

// Fixture setup and teardown
void test_merge_setup__initialize(void)
{
	repo = cl_git_sandbox_init(TEST_REPO_PATH);
    git_repository_index(&repo_index, repo);
}

void test_merge_setup__cleanup(void)
{
    git_index_free(repo_index);
	cl_git_sandbox_cleanup();
}

static void write_file_contents(const char *filename, const char *output)
{
	git_buf file_path_buf = GIT_BUF_INIT;

    git_buf_printf(&file_path_buf, "%s/%s", git_repository_path(repo), filename);
	cl_git_rewritefile(file_path_buf.ptr, output);

	git_buf_free(&file_path_buf);
}

struct merge_head_cb_data {
	const char **oid_str;
	unsigned int len;

	unsigned int i;
};

static int merge_head_foreach_cb(const git_oid *oid, void *payload)
{
	git_oid expected_oid;
	struct merge_head_cb_data *cb_data = payload;

	git_oid_fromstr(&expected_oid, cb_data->oid_str[cb_data->i]);
	cl_assert(git_oid_cmp(&expected_oid, oid) == 0);
	cb_data->i++;
	return 0;
}

void test_merge_setup__head_notfound(void)
{
	int error;

	cl_git_fail((error = git_repository_mergehead_foreach(repo,
		merge_head_foreach_cb, NULL)));
	cl_assert(error == GIT_ENOTFOUND);
}

void test_merge_setup__head_invalid_oid(void)
{
	int error;

	write_file_contents(GIT_MERGE_HEAD_FILE, "invalid-oid\n");

	cl_git_fail((error = git_repository_mergehead_foreach(repo,
		merge_head_foreach_cb, NULL)));
	cl_assert(error == -1);
}

void test_merge_setup__head_foreach_nonewline(void)
{
	int error;

	write_file_contents(GIT_MERGE_HEAD_FILE, THEIRS_SIMPLE_OID);

	cl_git_fail((error = git_repository_mergehead_foreach(repo,
		merge_head_foreach_cb, NULL)));
	cl_assert(error == -1);
}

void test_merge_setup__head_foreach_one(void)
{
	const char *expected = THEIRS_SIMPLE_OID;

	struct merge_head_cb_data cb_data = { &expected, 1 };

	write_file_contents(GIT_MERGE_HEAD_FILE, THEIRS_SIMPLE_OID "\n");

	cl_git_pass(git_repository_mergehead_foreach(repo,
		merge_head_foreach_cb, &cb_data));

	cl_assert(cb_data.i == cb_data.len);
}

void test_merge_setup__head_foreach_octopus(void)
{
	const char *expected[] = { THEIRS_SIMPLE_OID,
		OCTO1_OID, OCTO2_OID, OCTO3_OID, OCTO4_OID, OCTO5_OID };

	struct merge_head_cb_data cb_data = { expected, 6 };

	write_file_contents(GIT_MERGE_HEAD_FILE,
		THEIRS_SIMPLE_OID "\n"
		OCTO1_OID "\n"
		OCTO2_OID "\n"
		OCTO3_OID "\n"
		OCTO4_OID "\n"
		OCTO5_OID "\n");

	cl_git_pass(git_repository_mergehead_foreach(repo,
		merge_head_foreach_cb, &cb_data));

	cl_assert(cb_data.i == cb_data.len);
}
