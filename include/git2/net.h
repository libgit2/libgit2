#ifndef INCLUDE_net_h__
#define INCLUDE_net_h__

#include "common.h"
#include "oid.h"
#include "types.h"

#define GIT_DEFAULT_PORT "9418"

/*
 * We need this because we need to know whether we should call
 * git-upload-pack or git-receive-pack on the remote end when get_refs
 * gets called.
 */

#define GIT_DIR_FETCH 0
#define GIT_DIR_PUSH 1

/*
 * This is what we give out on ->ls()
 */

struct git_remote_head {
	git_oid oid;
	char *name;
};

struct git_headarray {
	unsigned int len;
	struct git_remote_head **heads;
};

#endif
