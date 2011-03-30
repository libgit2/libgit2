#ifndef INCLUDE_config_h__
#define INCLUDE_config_h__

#include "git2/config.h"

struct git_config {
	char *file_path;

	struct {
		gitfo_buf buffer;
		char *read_ptr;
		int line_number;
		int eof;
	} reader;

	git_cvar *vars;
	git_cvar *vars_tail;
};

struct git_cvar {
	git_cvar *next;
	char *name;
	char *value;
};

/*
 * If you're going to delete something inside this loop, it's such a
 * hassle that you should use the for-loop directly.
 */
#define CVAR_LIST_FOREACH(start, iter) \
	for ((iter) = (start); (iter) != NULL; (iter) = (iter)->next)

#endif
