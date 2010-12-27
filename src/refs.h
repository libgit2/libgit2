#ifndef INCLUDE_refs_h__
#define INCLUDE_refs_h__

#include "common.h"
#include "git2/oid.h"

struct git_reference {
	git_rtype type;
	char *name;
};

#endif