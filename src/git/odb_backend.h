#ifndef INCLUDE_git_odb_backend_h__
#define INCLUDE_git_odb_backend_h__

#include "common.h"
#include "types.h"
#include "oid.h"

GIT_BEGIN_DECL

struct git_odb_backend {
	git_odb *odb;

	int priority;

	int (* read)(
			git_rawobj *,
			struct git_odb_backend *,
			const git_oid *);

	int (* read_header)(
			git_rawobj *,
			struct git_odb_backend *,
			const git_oid *);

	int (* write)(
			git_oid *id,
			struct git_odb_backend *,
			git_rawobj *obj);

	int (* exists)(
			struct git_odb_backend *,
			const git_oid *);

	void (* free)(struct git_odb_backend *);

};

GIT_END_DECL

#endif
