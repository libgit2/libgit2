#include "clar_libgit2.h"
#include "git2/import.h"

static git_repository *repo = NULL;

static char *test_data = "Hello, world.";

void test_importer_blob__initialize(void)
{
	repo = cl_git_sandbox_init("empty_standard_repo");
}

void test_importer_blob__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_importer_blob__create_importer(void)
{
	git_importer *importer;

	cl_git_pass(git_importer_create(&importer, repo));

	cl_git_pass(git_importer_free(importer));
}

static int basic_cb(
	void *payload, const git_oid *oid, const void *blob, size_t length)
{
	GIT_UNUSED(oid);

	cl_assert(length == strlen((char *)payload));
	cl_assert(memcmp(payload, blob, strlen((char *)payload)) == 0);

	return 0;
}

void test_importer_blob__basic(void)
{
	git_importer *importer;

	cl_git_pass(git_importer_create(&importer, repo));

	cl_git_pass(git_importer_blob(importer));

	cl_git_pass(git_importer_mark(importer, 1));

	cl_git_pass(git_importer_data(importer, test_data, strlen(test_data)));

	cl_git_pass(
		git_importer_cat_blob_from_mark(importer, 1, basic_cb, test_data));

	cl_git_pass(git_importer_free(importer));
}


void test_importer_blob__multiple(void)
{
	git_importer *importer;
	static char *data2 = "Some more data";
	static char *data3 = "Even more data";

	cl_git_pass(git_importer_create(&importer, repo));

	cl_git_pass(git_importer_blob(importer));
	cl_git_pass(git_importer_mark(importer, 1));
	cl_git_pass(git_importer_data(importer, test_data, strlen(test_data)));

	cl_git_pass(git_importer_blob(importer));
	cl_git_pass(git_importer_data(importer, data2, strlen(data2)));

	cl_git_pass(git_importer_blob(importer));
	cl_git_pass(git_importer_mark(importer, 3));
	cl_git_pass(git_importer_data(importer, data3, strlen(data3)));

	cl_git_pass(
		git_importer_cat_blob_from_mark(importer, 1, basic_cb, test_data));

	cl_git_pass(
		git_importer_cat_blob_from_mark(importer, 3, basic_cb, data3));

	cl_git_pass(git_importer_free(importer));
}

