#include "clar_libgit2.h"
#include "checkout_helpers.h"
#include "refs.h"
#include "fileops.h"

/* this is essentially the code from git__unescape modified slightly */
void strip_cr_from_buf(git_buf *buf)
{
	char *scan, *pos = buf->ptr, *end = pos + buf->size;

	for (scan = pos; scan < end; pos++, scan++) {
		if (*scan == '\r')
			scan++; /* skip '\r' */
		if (pos != scan)
			*pos = *scan;
	}

	*pos = '\0';
	buf->size = (pos - buf->ptr);
}

void assert_on_branch(git_repository *repo, const char *branch)
{
	git_reference *head;
	git_buf bname = GIT_BUF_INIT;

	cl_git_pass(git_reference_lookup(&head, repo, GIT_HEAD_FILE));
	cl_assert_(git_reference_type(head) == GIT_REF_SYMBOLIC, branch);

	cl_git_pass(git_buf_joinpath(&bname, "refs/heads", branch));
	cl_assert_equal_s(bname.ptr, git_reference_symbolic_target(head));

	git_reference_free(head);
	git_buf_free(&bname);
}

void reset_index_to_treeish(git_object *treeish)
{
	git_object *tree;
	git_index *index;
	git_repository *repo = git_object_owner(treeish);

	cl_git_pass(git_object_peel(&tree, treeish, GIT_OBJ_TREE));

	cl_git_pass(git_repository_index(&index, repo));
	cl_git_pass(git_index_read_tree(index, (git_tree *)tree));
	cl_git_pass(git_index_write(index));

	git_object_free(tree);
	git_index_free(index);
}

static void check_file_contents_internal(
	const char *path,
	const char *expected_content,
	bool strip_cr,
	const char *file,
	int line,
	const char *msg)
{
	int fd;
	char data[1024] = {0};
	git_buf buf = GIT_BUF_INIT;
	size_t expected_len = expected_content ? strlen(expected_content) : 0;

	fd = p_open(path, O_RDONLY);
	cl_assert(fd >= 0);

	buf.ptr = data;
	buf.size = p_read(fd, buf.ptr, sizeof(data));

	cl_git_pass(p_close(fd));

	if (strip_cr)
		strip_cr_from_buf(&buf);

	clar__assert_equal_i((int)expected_len, (int)buf.size, file, line, "strlen(expected_content) != strlen(actual_content)", 1);
	clar__assert_equal_s(expected_content, buf.ptr, file, line, msg, 1);
}

void check_file_contents_at_line(
	const char *path, const char *expected,
	const char *file, int line, const char *msg)
{
	check_file_contents_internal(path, expected, false, file, line, msg);
}

void check_file_contents_nocr_at_line(
	const char *path, const char *expected,
	const char *file, int line, const char *msg)
{
	check_file_contents_internal(path, expected, true, file, line, msg);
}
