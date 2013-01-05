#include "buffer.h"
#include "git2/object.h"
#include "git2/repository.h"

extern void strip_cr_from_buf(git_buf *buf);
extern void assert_on_branch(git_repository *repo, const char *branch);
extern void reset_index_to_treeish(git_object *treeish);
extern void test_file_contents(const char *path, const char *expected);
extern void test_file_contents_nocr(const char *path, const char *expected);
