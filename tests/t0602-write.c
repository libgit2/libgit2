#include "test_lib.h"
#include "test_helpers.h"
#include "index.h"

#include <git/odb.h>
#include <git/index.h>

#define TEST_INDEX_PATH "../resources/index"

int filecmp(const char *filename1, const char *filename2)
{
	git_file file1, file2;
	struct stat stat1, stat2;

	/* char buffer1[1024], buffer2[1024]; */

	file1 = gitfo_open(filename1, O_RDONLY);
	file2 = gitfo_open(filename2, O_RDONLY);

	if (file1 < 0 || file2 < 0)
		return GIT_ERROR;

	gitfo_fstat(file1, &stat1);
	gitfo_fstat(file2, &stat2);

	if (stat1.st_size != stat2.st_size)
		return GIT_ERROR;

	/* TODO: byte-per-byte comparison */

	return 0;
}

BEGIN_TEST(index_load_test)
	git_index *index;
	git_filelock out_file;

	index = git_index_alloc(TEST_INDEX_PATH);
	must_be_true(index != NULL);
	must_pass(git_index_read(index));
	must_be_true(index->on_disk);

	must_pass(git_filelock_init(&out_file, "index_rewrite"));
	must_pass(git_filelock_lock(&out_file, 0));
	must_pass(git_index__write(index, &out_file));
	must_pass(git_filelock_commit(&out_file));

	git_index_free(index);
END_TEST
