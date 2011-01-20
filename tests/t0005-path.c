#include "test_lib.h"
#include "fileops.h"

static int ensure_normalized(const char *input_path, const char *expected_path)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_PATH_MAX];

	error = git_prettify_dir_path(buffer_out, input_path);
	if (error < GIT_SUCCESS)
		return error;

	if (expected_path == NULL)
		return error;

	if (strcmp(buffer_out, expected_path))
		error = GIT_ERROR;

	return error;
}

BEGIN_TEST(path_prettifying)
	must_pass(ensure_normalized("", ""));
	must_pass(ensure_normalized(".", ""));
	must_pass(ensure_normalized("./", ""));
	must_pass(ensure_normalized("./.", ""));
	must_fail(ensure_normalized("./..", NULL));
	must_fail(ensure_normalized("../.", NULL));
	must_fail(ensure_normalized("./.././/", NULL));
	must_pass(ensure_normalized("dir/..", ""));
	must_pass(ensure_normalized("dir/sub/../..", ""));
	must_pass(ensure_normalized("dir/sub/..///..", ""));
	must_pass(ensure_normalized("dir/sub///../..", ""));
	must_pass(ensure_normalized("dir/sub///..///..", ""));
	must_fail(ensure_normalized("dir/sub/../../..", NULL));
	must_pass(ensure_normalized("dir", "dir/"));
	must_pass(ensure_normalized("dir//", "dir/"));
	must_pass(ensure_normalized("./dir", "dir/"));
	must_pass(ensure_normalized("dir/.", "dir/"));
	must_pass(ensure_normalized("dir///./", "dir/"));
	must_pass(ensure_normalized("dir/sub/..", "dir/"));
	must_pass(ensure_normalized("dir//sub/..", "dir/"));
	must_pass(ensure_normalized("dir//sub/../", "dir/"));
	must_pass(ensure_normalized("dir/sub/../", "dir/"));
	must_pass(ensure_normalized("dir/sub/../.", "dir/"));
	must_pass(ensure_normalized("dir/s1/../s2/", "dir/s2/"));
	must_pass(ensure_normalized("d1/s1///s2/..//../s3/", "d1/s3/"));
	must_pass(ensure_normalized("d1/s1//../s2/../../d2", "d2/"));
	must_pass(ensure_normalized("dir/sub/../", "dir/"));
	
	must_pass(ensure_normalized("/", "/"));
	must_pass(ensure_normalized("//", "/"));
	must_pass(ensure_normalized("///", "/"));
	must_pass(ensure_normalized("/.", "/"));
	must_pass(ensure_normalized("/./", "/"));
	must_fail(ensure_normalized("/./..", NULL));
	must_fail(ensure_normalized("/../.", NULL));
	must_fail(ensure_normalized("/./.././/", NULL));
	must_pass(ensure_normalized("/dir/..", "/"));
	must_pass(ensure_normalized("/dir/sub/../..", "/"));
	must_fail(ensure_normalized("/dir/sub/../../..", NULL));
	must_pass(ensure_normalized("/dir", "/dir/"));
	must_pass(ensure_normalized("/dir//", "/dir/"));
	must_pass(ensure_normalized("/./dir", "/dir/"));
	must_pass(ensure_normalized("/dir/.", "/dir/"));
	must_pass(ensure_normalized("/dir///./", "/dir/"));
	must_pass(ensure_normalized("/dir//sub/..", "/dir/"));
	must_pass(ensure_normalized("/dir/sub/../", "/dir/"));
	must_pass(ensure_normalized("//dir/sub/../.", "/dir/"));
	must_pass(ensure_normalized("/dir/s1/../s2/", "/dir/s2/"));
	must_pass(ensure_normalized("/d1/s1///s2/..//../s3/", "/d1/s3/"));
	must_pass(ensure_normalized("/d1/s1//../s2/../../d2", "/d2/"));
END_TEST
