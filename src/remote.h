#ifndef INCLUDE_remote_h__
#define INCLUDE_remote_h__

#include "refspec.h"
#include "transport.h"
#include "repository.h"

struct git_remote {
	char *name;
	char *url;
	git_headarray refs;
	struct git_refspec fetch;
	struct git_refspec push;
	git_transport *transport;
	git_repository *repo;
	int need_pack:1;
};

#endif
