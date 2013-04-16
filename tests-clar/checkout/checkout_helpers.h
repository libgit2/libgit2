#include "buffer.h"
#include "git2/object.h"
#include "git2/repository.h"

extern void strip_cr_from_buf(git_buf *buf);
extern void assert_on_branch(git_repository *repo, const char *branch);
extern void reset_index_to_treeish(git_object *treeish);

extern void check_file_contents_at_line(
	const char *path, const char *expected,
	const char *file, int line, const char *msg);

extern void check_file_contents_nocr_at_line(
	const char *path, const char *expected,
	const char *file, int line, const char *msg);

#define check_file_contents(PATH,EXP) \
	check_file_contents_at_line(PATH,EXP,__FILE__,__LINE__,"String mismatch: " #EXP " != " #PATH)

#define check_file_contents_nocr(PATH,EXP) \
	check_file_contents_nocr_at_line(PATH,EXP,__FILE__,__LINE__,"String mismatch: " #EXP " != " #PATH)
