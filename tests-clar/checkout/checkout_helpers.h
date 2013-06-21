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

typedef struct {
	int n_conflicts;
	int n_dirty;
	int n_updates;
	int n_untracked;
	int n_ignored;
	int debug;
} checkout_counts;

extern int checkout_count_callback(
	git_checkout_notify_t why,
	const char *path,
	const git_diff_file *baseline,
	const git_diff_file *target,
	const git_diff_file *workdir,
	void *payload);
