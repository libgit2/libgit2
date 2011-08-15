#ifndef INCLUDE_reflog_h__
#define INCLUDE_reflog_h__

#include "common.h"
#include "git2/reflog.h"
#include "vector.h"

#define GIT_REFLOG_DIR "logs/"

#define GIT_REFLOG_SIZE_MIN (2*GIT_OID_HEXSZ+2+17)

struct git_reflog_entry {
	git_oid oid_old;
	git_oid oid_cur;

	git_signature *committer;

	char *msg;
};

struct git_reflog {
	char *ref_name;
	git_vector entries;
};

#endif /* INCLUDE_reflog_h__ */
