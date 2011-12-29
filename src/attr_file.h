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
#include "hashtable.h"

#define GIT_ATTR_FNMATCH_NEGATIVE	(1U << 0)
#define GIT_ATTR_FNMATCH_DIRECTORY	(1U << 1)
#define GIT_ATTR_FNMATCH_FULLPATH	(1U << 2)
#define GIT_ATTR_FNMATCH_MACRO		(1U << 3)

typedef struct {
	char *pattern;
	size_t length;
	unsigned int flags;
} git_attr_fnmatch;

typedef struct {
	git_refcount unused;
	const char *name;
    unsigned long name_hash;
} git_attr_name;

typedef struct {
	git_refcount rc;			/* for macros */
	char *name;
    unsigned long name_hash;
    const char *value;
	int is_allocated;
} git_attr_assignment;

typedef struct {
	git_attr_fnmatch match;
	git_vector assigns;			/* vector of <git_attr_assignment*> */
} git_attr_rule;

typedef struct {
	char *path;					/* cache the path this was loaded from */
	git_vector rules;			/* vector of <git_attr_rule*> */
} git_attr_file;

typedef struct {
	const char *path;
	const char *basename;
	int is_dir;
} git_attr_path;

typedef struct {
	int initialized;
	git_hashtable *files;	  /* hash path to git_attr_file */
	git_hashtable *macros;	  /* hash name to vector<git_attr_assignment> */
} git_attr_cache;

/*
 * git_attr_file API
 */

extern int git_attr_file__from_buffer(
	git_repository *repo, const char *buf, git_attr_file **out);
extern int git_attr_file__from_file(
	git_repository *repo, const char *path, git_attr_file **out);

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

extern void git_attr_rule__free(git_attr_rule *rule);

extern int git_attr_rule__match_path(
	git_attr_rule *rule,
	const git_attr_path *path);

extern git_attr_assignment *git_attr_rule__lookup_assignment(
	git_attr_rule *rule, const char *name);

extern int git_attr_path__init(
	git_attr_path *info, const char *path);

extern int git_attr_assignment__parse(
	git_repository *repo, /* needed to expand macros */
	git_vector *assigns,
	const char **scan);

extern int git_attr_cache__insert_macro(
	git_repository *repo, git_attr_rule *macro);

#endif
