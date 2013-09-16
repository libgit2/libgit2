
#ifndef INCLUDE_blame_git__
#define INCLUDE_blame_git__

#include "git2.h"
#include "xdiff/xinclude.h"
#include "blame.h"

/*
 * One blob in a commit that is being suspected
 */
struct origin {
	int refcnt;
	struct origin *previous;
	git_commit *commit;
	git_blob *blob;
	char path[];
};

/*
 * Each group of lines is described by a blame_entry; it can be split
 * as we pass blame to the parents.  They form a linked list in the
 * scoreboard structure, sorted by the target line number.
 */
struct blame_entry {
	struct blame_entry *prev;
	struct blame_entry *next;

	/* the first line of this group in the final image;
	 * internally all line numbers are 0 based.
	 */
	int lno;

	/* how many lines this group has */
	int num_lines;

	/* the commit that introduced this group into the final image */
	struct origin *suspect;

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
};

/*
 * The current state of the blame assignment.
 */
struct scoreboard {
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
	struct blame_entry *ent;

	/* look-up a line in the final buffer */
	int num_lines;

	git_blame *blame;
};


int get_origin(struct origin **out, struct scoreboard *sb, git_commit *commit, const char *path);
int make_origin(struct origin **out, git_commit *commit, const char *path);
struct origin *origin_incref(struct origin *o);
void origin_decref(struct origin *o);
void assign_blame(struct scoreboard *sb, uint32_t flags);
void coalesce(struct scoreboard *sb);

#endif
