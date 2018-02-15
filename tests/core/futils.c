#include "clar_libgit2.h"
#include "fileops.h"

// Fixture setup and teardown
void test_core_futils__initialize(void)
{
	cl_must_pass(p_mkdir("futils", 0777));
}

void test_core_futils__cleanup(void)
{
	cl_fixture_cleanup("futils");
}

void test_core_futils__writebuffer(void)
{
	git_buf out = GIT_BUF_INIT,
		append = GIT_BUF_INIT;

	/* create a new file */
	git_buf_puts(&out, "hello!\n");
	git_buf_printf(&out, "this is a %s\n", "test");

	cl_git_pass(git_futils_writebuffer(&out, "futils/test-file", O_RDWR|O_CREAT, 0666));

	cl_assert_equal_file(out.ptr, out.size, "futils/test-file");

	/* append some more data */
	git_buf_puts(&append, "And some more!\n");
	git_buf_put(&out, append.ptr, append.size);

	cl_git_pass(git_futils_writebuffer(&append, "futils/test-file", O_RDWR|O_APPEND, 0666));

	cl_assert_equal_file(out.ptr, out.size, "futils/test-file");

	git_buf_free(&out);
	git_buf_free(&append);
}

void test_core_futils__write_hidden_file(void)
{
#ifndef GIT_WIN32
	cl_skip();
#else
	git_buf out = GIT_BUF_INIT, append = GIT_BUF_INIT;
	bool hidden;

	git_buf_puts(&out, "hidden file.\n");
	git_futils_writebuffer(&out, "futils/test-file", O_RDWR | O_CREAT, 0666);

	cl_git_pass(git_win32__set_hidden("futils/test-file", true));

	/* append some more data */
	git_buf_puts(&append, "And some more!\n");
	git_buf_put(&out, append.ptr, append.size);

	cl_git_pass(git_futils_writebuffer(&append, "futils/test-file", O_RDWR | O_APPEND, 0666));

	cl_assert_equal_file(out.ptr, out.size, "futils/test-file");

	cl_git_pass(git_win32__hidden(&hidden, "futils/test-file"));
	cl_assert(hidden);

	git_buf_free(&out);
	git_buf_free(&append);
#endif
}

typedef enum {
	mode_cmp_true = 0,
	mode_cmp_false,
	mode_cmp_false_if_strict,
} mode_cmp_expected;

struct mode_cmp {
	git_filemode_t a;
	git_filemode_t b;
	mode_cmp_expected expected;
};

void test_core_futils__mode_compare(void)
{
	int lineno = __LINE__ + 2;
	struct mode_cmp cmps[] = {
		{GIT_FILEMODE_BLOB, GIT_FILEMODE_BLOB, mode_cmp_true},
		{GIT_FILEMODE_BLOB, GIT_FILEMODE_TREE, mode_cmp_false},
		{GIT_FILEMODE_TREE, GIT_FILEMODE_TREE, mode_cmp_true},
		{GIT_FILEMODE_BLOB_EXECUTABLE, GIT_FILEMODE_BLOB_EXECUTABLE, mode_cmp_true},
		{GIT_FILEMODE_BLOB_EXECUTABLE, GIT_FILEMODE_BLOB, mode_cmp_false_if_strict},
		{0100755, 0100766, mode_cmp_true},
		{0100777, 0100666, mode_cmp_false_if_strict},
	};
	size_t i;

	for (i = 0; i < 2 * ARRAY_SIZE(cmps); i++) {
		int testno = i / 2;
		struct mode_cmp cmp = cmps[testno];
		bool strict = (i % 2 == 0 ? true : false);

		bool result_a = git__is_filemode_equal(strict, cmp.a, cmp.b);
		bool result_b = git__is_filemode_equal(strict, cmp.b, cmp.a);

		cl_assert_equal_b(result_a, result_b);

		switch (cmp.expected) {
			case mode_cmp_true:
				cl_assert_at_line(result_a, __FILE__, lineno + testno);
				break;
			case mode_cmp_false:
				cl_assert_at_line(!result_a, __FILE__, lineno + testno);
				break;
			case mode_cmp_false_if_strict:
#ifdef GIT_WIN32
				/* Windows has no concept of exec, hence strict mode is irrelevant */
				cl_assert_at_line(result_a, __FILE__, lineno + testno);
#else
				cl_assert_at_line(result_a == !strict, __FILE__, lineno + testno);
#endif
				break;
		}
	}
}
