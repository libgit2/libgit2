#include "clar_libgit2.h"
#include "buffer.h"
#include "posix.h"
#include "index.h"

static git_repository *g_repo = NULL;

void test_index_filemodes__initialize(void)
{
	g_repo = cl_git_sandbox_init("filemodes");
}

void test_index_filemodes__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_index_filemodes__read(void)
{
	git_index *index;
	unsigned int i;
	static bool expected[6] = { 0, 1, 0, 1, 0, 1 };

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_assert_equal_i(6, (int)git_index_entrycount(index));

	for (i = 0; i < 6; ++i) {
		const git_index_entry *entry = git_index_get_byindex(index, i);
		cl_assert(entry != NULL);
		cl_assert(((entry->mode & 0100) ? 1 : 0) == expected[i]);
	}

	git_index_free(index);
}

static void replace_file_with_mode(
	const char *filename, const char *backup, unsigned int create_mode)
{
	git_buf path = GIT_BUF_INIT, content = GIT_BUF_INIT;

	cl_git_pass(git_buf_joinpath(&path, "filemodes", filename));
	cl_git_pass(git_buf_printf(&content, "%s as %08u (%d)",
		filename, create_mode, rand()));

	cl_git_pass(p_rename(path.ptr, backup));
	cl_git_write2file(
		path.ptr, content.ptr, O_WRONLY|O_CREAT|O_TRUNC, create_mode);

	git_buf_free(&path);
	git_buf_free(&content);
}

static void add_and_check_mode(
	git_index *index, const char *filename, unsigned int expect_mode)
{
	size_t pos;
	const git_index_entry *entry;

	cl_git_pass(git_index_add_bypath(index, filename));

	cl_assert(!git_index_find(&pos, index, filename));

	entry = git_index_get_byindex(index, pos);
	cl_assert(entry->mode == expect_mode);
}

void test_index_filemodes__untrusted(void)
{
	git_config *cfg;
	git_index *index;
	bool can_filemode = cl_is_chmod_supported();

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "core.filemode", false));
	git_config_free(cfg);

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_assert((git_index_caps(index) & GIT_INDEXCAP_NO_FILEMODE) != 0);

	/* 1 - add 0644 over existing 0644 -> expect 0644 */
	replace_file_with_mode("exec_off", "filemodes/exec_off.0", 0644);
	add_and_check_mode(index, "exec_off", GIT_FILEMODE_BLOB);

	/* 2 - add 0644 over existing 0755 -> expect 0755 */
	replace_file_with_mode("exec_on", "filemodes/exec_on.0", 0644);
	add_and_check_mode(index, "exec_on", GIT_FILEMODE_BLOB_EXECUTABLE);

	/* 3 - add 0755 over existing 0644 -> expect 0644 */
	replace_file_with_mode("exec_off", "filemodes/exec_off.1", 0755);
	add_and_check_mode(index, "exec_off", GIT_FILEMODE_BLOB);

	/* 4 - add 0755 over existing 0755 -> expect 0755 */
	replace_file_with_mode("exec_on", "filemodes/exec_on.1", 0755);
	add_and_check_mode(index, "exec_on", GIT_FILEMODE_BLOB_EXECUTABLE);

	/*  5 - add new 0644 -> expect 0644 */
	cl_git_write2file("filemodes/new_off", "blah",
		O_WRONLY | O_CREAT | O_TRUNC, 0644);
	add_and_check_mode(index, "new_off", GIT_FILEMODE_BLOB);

	/* this test won't give predictable results on a platform
	 * that doesn't support filemodes correctly, so skip it.
	 */
	if (can_filemode) {
		/* 6 - add 0755 -> expect 0755 */
		cl_git_write2file("filemodes/new_on", "blah",
			O_WRONLY | O_CREAT | O_TRUNC, 0755);
		add_and_check_mode(index, "new_on", GIT_FILEMODE_BLOB_EXECUTABLE);
	}

	git_index_free(index);
}

void test_index_filemodes__trusted(void)
{
	git_config *cfg;
	git_index *index;

	/* Only run these tests on platforms where I can actually
	 * chmod a file and get the stat results I expect!
	 */
	if (!cl_is_chmod_supported())
		return;

	cl_git_pass(git_repository_config(&cfg, g_repo));
	cl_git_pass(git_config_set_bool(cfg, "core.filemode", true));
	git_config_free(cfg);

	cl_git_pass(git_repository_index(&index, g_repo));
	cl_assert((git_index_caps(index) & GIT_INDEXCAP_NO_FILEMODE) == 0);

	/* 1 - add 0644 over existing 0644 -> expect 0644 */
	replace_file_with_mode("exec_off", "filemodes/exec_off.0", 0644);
	add_and_check_mode(index, "exec_off", GIT_FILEMODE_BLOB);

	/* 2 - add 0644 over existing 0755 -> expect 0644 */
	replace_file_with_mode("exec_on", "filemodes/exec_on.0", 0644);
	add_and_check_mode(index, "exec_on", GIT_FILEMODE_BLOB);

	/* 3 - add 0755 over existing 0644 -> expect 0755 */
	replace_file_with_mode("exec_off", "filemodes/exec_off.1", 0755);
	add_and_check_mode(index, "exec_off", GIT_FILEMODE_BLOB_EXECUTABLE);

	/* 4 - add 0755 over existing 0755 -> expect 0755 */
	replace_file_with_mode("exec_on", "filemodes/exec_on.1", 0755);
	add_and_check_mode(index, "exec_on", GIT_FILEMODE_BLOB_EXECUTABLE);

	/*  5 - add new 0644 -> expect 0644 */
	cl_git_write2file("filemodes/new_off", "blah",
		O_WRONLY | O_CREAT | O_TRUNC, 0644);
	add_and_check_mode(index, "new_off", GIT_FILEMODE_BLOB);

	/* 6 - add 0755 -> expect 0755 */
	cl_git_write2file("filemodes/new_on", "blah",
		O_WRONLY | O_CREAT | O_TRUNC, 0755);
	add_and_check_mode(index, "new_on", GIT_FILEMODE_BLOB_EXECUTABLE);

	git_index_free(index);
}
