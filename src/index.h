#ifndef INCLUDE_index_h__
#define INCLUDE_index_h__

#include "fileops.h"
#include "filelock.h"
#include "git/odb.h"
#include "git/index.h"

#define GIT_IDXENTRY_NAMEMASK  (0x0fff)
#define GIT_IDXENTRY_STAGEMASK (0x3000)
#define GIT_IDXENTRY_EXTENDED  (0x4000)
#define GIT_IDXENTRY_VALID     (0x8000)
#define GIT_IDXENTRY_STAGESHIFT 12

typedef struct {
	uint32_t seconds;
	uint32_t nanoseconds;
} git_index_time;

struct git_index_entry {
	git_index_time ctime;
	git_index_time mtime;

	uint32_t dev;
	uint32_t ino;
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t file_size;

	git_oid oid;

	uint16_t flags;
	uint16_t flags_extended;

	char *path;
};


struct git_index_tree {
	char *name;

	struct git_index_tree *parent;
	struct git_index_tree **children;
	size_t children_count;

	size_t entries;
	git_oid oid;
};

typedef struct git_index_tree git_index_tree;

struct git_index {

	char *index_file_path;
	time_t last_modified;

	git_index_entry *entries;
	unsigned int entries_size;

	unsigned int entry_count;
	unsigned int sorted:1,
				 on_disk:1;

	git_index_tree *tree;
};

int git_index__write(git_index *index, git_filelock *file);
void git_index__sort(git_index *index);
int git_index__parse(git_index *index, const char *buffer, size_t buffer_size);
int git_index__remove_pos(git_index *index, unsigned int position);
int git_index__append(git_index *index, const git_index_entry *entry);

void git_index_tree__free(git_index_tree *tree);

#endif
