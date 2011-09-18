/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_refspec_h__
#define INCLUDE_refspec_h__

#include "git2/refspec.h"

struct git_refspec {
	int force;
	char *src;
	char *dst;
};

int git_refspec_parse(struct git_refspec *refspec, const char *str);

#endif
