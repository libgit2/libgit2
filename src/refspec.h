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
