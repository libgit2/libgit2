/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attr_file_h__
#define INCLUDE_attr_file_h__

#include "git2/attr.h"
#include "vector.h"

typedef struct {
	char *pattern;
	size_t length;
	int negative;
	int directory;
	int fullpath;
} git_attr_fnmatch;

typedef struct {
	const char *name;
	unsigned long name_hash;
} git_attr_name;

typedef struct {
	char *name;
    unsigned long name_hash;
	size_t name_len;
    const char *value;
	int is_allocated;
} git_attr_assignment;

typedef struct {
	git_attr_fnmatch match;
	git_vector assigns; /* <git_attr_assignment*> */
} git_attr_rule;

typedef struct {
	char *path;
	git_vector rules; /* <git_attr_rule*> */
} git_attr_file;

typedef struct {
	const char *path;
	const char *basename;
	int is_dir;
} git_attr_path;

/*
 * git_attr_file API
 */

extern int git_attr_file__from_buffer(git_attr_file **out, const char *buf);
extern int git_attr_file__from_file(git_attr_file **out, const char *path);

extern void git_attr_file__free(git_attr_file *file);

extern int git_attr_file__lookup_one(
	git_attr_file *file,
	const git_attr_path *path,
	const char *attr,
	const char **value);

/* loop over rules in file from bottom to top */
#define git_attr_file__foreach_matching_rule(file, path, iter, rule)	\
	git_vector_rforeach(&(file)->rules, (iter), (rule)) \
		if (git_attr_rule__match_path((rule), (path)) == GIT_SUCCESS)

extern unsigned long git_attr_file__name_hash(const char *name);


/*
 * other utilities
 */

extern int git_attr_rule__match_path(
	git_attr_rule *rule,
	const git_attr_path *path);

extern git_attr_assignment *git_attr_rule__lookup_assignment(
	git_attr_rule *rule, const char *name);

extern int git_attr_path__init(
	git_attr_path *info, const char *path);

#endif
