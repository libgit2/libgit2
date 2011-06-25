#ifndef INCLUDE_net_h__
#define INCLUDE_net_h__

#include "common.h"
#include "oid.h"
#include "types.h"

/*
 * We need this because we need to know whether we should call
 * git-upload-pack or git-receive-pack on the remote end when get_refs
 * gets called.
 */

enum git_net_direction {
	INTENT_PUSH,
	INTENT_PULL
};

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
