/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refspec_h__
#define INCLUDE_refspec_h__

#include "git2/refspec.h"
#include "buffer.h"

struct git_refspec {
	struct git_refspec *next;
	char *src;
	char *dst;
	unsigned int force :1,
		pattern :1,
		matching :1;
};

int git_refspec_parse(struct git_refspec *refspec, const char *str);

/**
 * Transform a reference to its target following the refspec's rules,
 * and writes the results into a git_buf.
 *
 * @param out where to store the target name
 * @param spec the refspec
 * @param name the name of the reference to transform
 * @return 0 or error if buffer allocation fails
 */
int git_refspec_transform_r(git_buf *out, const git_refspec *spec, const char *name);

#endif
