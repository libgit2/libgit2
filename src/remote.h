#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "remote.h"
#include "refspec.h"
#include "transport.h"

struct git_remote {
	char *name;
	char *url;
	struct git_refspec fetch;
	struct git_refspec push;
	git_transport *transport;
};

int git_remote_send_wants(git_remote *remote, git_headarray *list);

#endif
