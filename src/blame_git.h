
#ifndef INCLUDE_blame_git__
#define INCLUDE_blame_git__

#include "git2.h"
#include "xdiff/xinclude.h"
#include "blame.h"

/*
 * One blob in a commit that is being suspected
 */
typedef struct git_blame__origin {
	int refcnt;
	struct git_blame__origin *previous;
	git_commit *commit;
	git_blob *blob;
	char path[];
} git_blame__origin;

/*
 * Each group of lines is described by a git_blame__entry; it can be split
 * as we pass blame to the parents.  They form a linked list in the
 * scoreboard structure, sorted by the target line number.
 */
typedef struct git_blame__entry {
	struct git_blame__entry *prev;
	struct git_blame__entry *next;

	/* the first line of this group in the final image;
	 * internally all line numbers are 0 based.
	 */
	int lno;

	/* how many lines this group has */
	int num_lines;

	/* the commit that introduced this group into the final image */
	git_blame__origin *suspect;

	/* true if the suspect is truly guilty; false while we have not
	 * checked if the group came from one of its parents.
	 */
	char guilty;

	/* true if the entry has been scanned for copies in the current parent
	 */
	char scanned;

	/* the line number of the first line of this group in the
	 * suspect's file; internally all line numbers are 0 based.
	 */
	int s_lno;

	/* how significant this entry is -- cached to avoid
	 * scanning the lines over and over.
	 */
	unsigned score;

	/* Whether this entry has been tracked to a boundary commit.
	 */
	bool is_boundary;
} git_blame__entry;

/*
 * The current state of the blame assignment.
 */
typedef struct git_blame__scoreboard {
	/* the final commit (i.e. where we started digging from) */
	git_commit *final;
	const char *path;

	/*
	 * The contents in the final image.
	 * Used by many functions to obtain contents of the nth line,
	 * indexed with scoreboard.lineno[blame_entry.lno].
	 */
	const char *final_buf;
	git_off_t final_buf_size;

	/* linked list of blames */
	git_blame__entry *ent;

	/* look-up a line in the final buffer */
	int num_lines;

	git_blame *blame;
} git_blame__scoreboard;


int get_origin(git_blame__origin **out, git_blame__scoreboard *sb, git_commit *commit, const char *path);
int make_origin(git_blame__origin **out, git_commit *commit, const char *path);
git_blame__origin *origin_incref(git_blame__origin *o);
void origin_decref(git_blame__origin *o);
void assign_blame(git_blame__scoreboard *sb, uint32_t flags);
void coalesce(git_blame__scoreboard *sb);

#endif
