#include "test_lib.h"
#include "fileops.h"

typedef int (normalize_path)(char *, const char *);

static int ensure_normalized(const char *input_path, const char *expected_path, normalize_path normalizer)
{
	int error = GIT_SUCCESS;
	char buffer_out[GIT_PATH_MAX];

	error = normalizer(buffer_out, input_path);
	if (error < GIT_SUCCESS)
		return error;

	if (expected_path == NULL)
		return error;

	if (strcmp(buffer_out, expected_path))
		error = GIT_ERROR;

	return error;
}

static int ensure_dir_path_normalized(const char *input_path, const char *expected_path)
{
	return ensure_normalized(input_path, expected_path, gitfo_prettify_dir_path);
}

static int ensure_file_path_normalized(const char *input_path, const char *expected_path)
{
	return ensure_normalized(input_path, expected_path, gitfo_prettify_file_path);
}

BEGIN_TEST(file_path_prettifying)
	must_pass(ensure_file_path_normalized("a", "a"));
	must_pass(ensure_file_path_normalized("./testrepo.git", "testrepo.git"));
	must_pass(ensure_file_path_normalized("./.git", ".git"));
	must_pass(ensure_file_path_normalized("./git.", "git."));
	must_fail(ensure_file_path_normalized("git./", NULL));
	must_fail(ensure_file_path_normalized("", NULL));
	must_fail(ensure_file_path_normalized(".", NULL));
	must_fail(ensure_file_path_normalized("./", NULL));
	must_fail(ensure_file_path_normalized("./.", NULL));
	must_fail(ensure_file_path_normalized("./..", NULL));
	must_fail(ensure_file_path_normalized("../.", NULL));
	must_fail(ensure_file_path_normalized("./.././/", NULL));
	must_fail(ensure_file_path_normalized("dir/..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/..///..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub///../..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub///..///..", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../../..", NULL));
	must_pass(ensure_file_path_normalized("dir", "dir"));
	must_fail(ensure_file_path_normalized("dir//", NULL));
	must_pass(ensure_file_path_normalized("./dir", "dir"));
	must_fail(ensure_file_path_normalized("dir/.", NULL));
	must_fail(ensure_file_path_normalized("dir///./", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/..", NULL));
	must_fail(ensure_file_path_normalized("dir//sub/..",NULL));
	must_fail(ensure_file_path_normalized("dir//sub/../", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("dir/sub/../.", NULL));
	must_fail(ensure_file_path_normalized("dir/s1/../s2/", NULL));
	must_fail(ensure_file_path_normalized("d1/s1///s2/..//../s3/", NULL));
	must_pass(ensure_file_path_normalized("d1/s1//../s2/../../d2", "d2"));
	must_fail(ensure_file_path_normalized("dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("....", NULL));
	must_fail(ensure_file_path_normalized("...", NULL));
	must_fail(ensure_file_path_normalized("./...", NULL));
	must_fail(ensure_file_path_normalized("d1/...", NULL));
	must_fail(ensure_file_path_normalized("d1/.../", NULL));
	must_fail(ensure_file_path_normalized("d1/.../d2", NULL));
	
	must_pass(ensure_file_path_normalized("/a", "/a"));
	must_pass(ensure_file_path_normalized("/./testrepo.git", "/testrepo.git"));
	must_pass(ensure_file_path_normalized("/./.git", "/.git"));
	must_pass(ensure_file_path_normalized("/./git.", "/git."));
	must_fail(ensure_file_path_normalized("/git./", NULL));
	must_fail(ensure_file_path_normalized("/", NULL));
	must_fail(ensure_file_path_normalized("/.", NULL));
	must_fail(ensure_file_path_normalized("/./", NULL));
	must_fail(ensure_file_path_normalized("/./.", NULL));
	must_fail(ensure_file_path_normalized("/./..", NULL));
	must_fail(ensure_file_path_normalized("/../.", NULL));
	must_fail(ensure_file_path_normalized("/./.././/", NULL));
	must_fail(ensure_file_path_normalized("/dir/..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/..///..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub///../..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub///..///..", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../../..", NULL));
	must_pass(ensure_file_path_normalized("/dir", "/dir"));
	must_fail(ensure_file_path_normalized("/dir//", NULL));
	must_pass(ensure_file_path_normalized("/./dir", "/dir"));
	must_fail(ensure_file_path_normalized("/dir/.", NULL));
	must_fail(ensure_file_path_normalized("/dir///./", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/..", NULL));
	must_fail(ensure_file_path_normalized("/dir//sub/..",NULL));
	must_fail(ensure_file_path_normalized("/dir//sub/../", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("/dir/sub/../.", NULL));
	must_fail(ensure_file_path_normalized("/dir/s1/../s2/", NULL));
	must_fail(ensure_file_path_normalized("/d1/s1///s2/..//../s3/", NULL));
	must_pass(ensure_file_path_normalized("/d1/s1//../s2/../../d2", "/d2"));
	must_fail(ensure_file_path_normalized("/dir/sub/../", NULL));
	must_fail(ensure_file_path_normalized("/....", NULL));
	must_fail(ensure_file_path_normalized("/...", NULL));
	must_fail(ensure_file_path_normalized("/./...", NULL));
	must_fail(ensure_file_path_normalized("/d1/...", NULL));
	must_fail(ensure_file_path_normalized("/d1/.../", NULL));
	must_fail(ensure_file_path_normalized("/d1/.../d2", NULL));
END_TEST

BEGIN_TEST(dir_path_prettifying)
	must_pass(ensure_dir_path_normalized("./testrepo.git", "testrepo.git/"));
	must_pass(ensure_dir_path_normalized("./.git", ".git/"));
	must_pass(ensure_dir_path_normalized("./git.", "git./"));
	must_pass(ensure_dir_path_normalized("git./", "git./"));
	must_pass(ensure_dir_path_normalized("", ""));
	must_pass(ensure_dir_path_normalized(".", ""));
	must_pass(ensure_dir_path_normalized("./", ""));
	must_pass(ensure_dir_path_normalized("./.", ""));
	must_fail(ensure_dir_path_normalized("./..", NULL));
	must_fail(ensure_dir_path_normalized("../.", NULL));
	must_fail(ensure_dir_path_normalized("./.././/", NULL));
	must_pass(ensure_dir_path_normalized("dir/..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub/../..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub/..///..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub///../..", ""));
	must_pass(ensure_dir_path_normalized("dir/sub///..///..", ""));
	must_fail(ensure_dir_path_normalized("dir/sub/../../..", NULL));
	must_pass(ensure_dir_path_normalized("dir", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//", "dir/"));
	must_pass(ensure_dir_path_normalized("./dir", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/.", "dir/"));
	must_pass(ensure_dir_path_normalized("dir///./", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/..", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//sub/..", "dir/"));
	must_pass(ensure_dir_path_normalized("dir//sub/../", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../.", "dir/"));
	must_pass(ensure_dir_path_normalized("dir/s1/../s2/", "dir/s2/"));
	must_pass(ensure_dir_path_normalized("d1/s1///s2/..//../s3/", "d1/s3/"));
	must_pass(ensure_dir_path_normalized("d1/s1//../s2/../../d2", "d2/"));
	must_pass(ensure_dir_path_normalized("dir/sub/../", "dir/"));
	must_fail(ensure_dir_path_normalized("....", NULL));
	must_fail(ensure_dir_path_normalized("...", NULL));
	must_fail(ensure_dir_path_normalized("./...", NULL));
	must_fail(ensure_dir_path_normalized("d1/...", NULL));
	must_fail(ensure_dir_path_normalized("d1/.../", NULL));
	must_fail(ensure_dir_path_normalized("d1/.../d2", NULL));

	must_pass(ensure_dir_path_normalized("/./testrepo.git", "/testrepo.git/"));
	must_pass(ensure_dir_path_normalized("/./.git", "/.git/"));
	must_pass(ensure_dir_path_normalized("/./git.", "/git./"));
	must_pass(ensure_dir_path_normalized("/git./", "/git./"));
	must_pass(ensure_dir_path_normalized("/", "/"));
	must_pass(ensure_dir_path_normalized("//", "/"));
	must_pass(ensure_dir_path_normalized("///", "/"));
	must_pass(ensure_dir_path_normalized("/.", "/"));
	must_pass(ensure_dir_path_normalized("/./", "/"));
	must_fail(ensure_dir_path_normalized("/./..", NULL));
	must_fail(ensure_dir_path_normalized("/../.", NULL));
	must_fail(ensure_dir_path_normalized("/./.././/", NULL));
	must_pass(ensure_dir_path_normalized("/dir/..", "/"));
	must_pass(ensure_dir_path_normalized("/dir/sub/../..", "/"));
	must_fail(ensure_dir_path_normalized("/dir/sub/../../..", NULL));
	must_pass(ensure_dir_path_normalized("/dir", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir//", "/dir/"));
	must_pass(ensure_dir_path_normalized("/./dir", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/.", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir///./", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir//sub/..", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/sub/../", "/dir/"));
	must_pass(ensure_dir_path_normalized("//dir/sub/../.", "/dir/"));
	must_pass(ensure_dir_path_normalized("/dir/s1/../s2/", "/dir/s2/"));
	must_pass(ensure_dir_path_normalized("/d1/s1///s2/..//../s3/", "/d1/s3/"));
	must_pass(ensure_dir_path_normalized("/d1/s1//../s2/../../d2", "/d2/"));
	must_fail(ensure_dir_path_normalized("/....", NULL));
	must_fail(ensure_dir_path_normalized("/...", NULL));
	must_fail(ensure_dir_path_normalized("/./...", NULL));
	must_fail(ensure_dir_path_normalized("/d1/...", NULL));
	must_fail(ensure_dir_path_normalized("/d1/.../", NULL));
	must_fail(ensure_dir_path_normalized("/d1/.../d2", NULL));
END_TEST
