#include "refs.h"
#include "git2/refs.h"
#include "git2/gitup_refs.h"
#include "repository.h"
#include "filebuf.h"
#include "pack.h"
#include "reflog.h"
#include "refdb.h"

#include <git2/oid.h>
#include <git2/sys/refs.h>

int gitup_reference_create_virtual(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const git_oid *id)
{
	int force = false;
	const char *log_message = "";
	return git_reference_create(ref_out, repo, name, id, force, log_message);
}

int gitup_reference_symbolic_create_virtual(
	git_reference **ref_out,
	git_repository *repo,
	const char *name,
	const char *target)
{
	int force = false;
	const char *log_message = "";
	return git_reference_symbolic_create(ref_out, repo, name, target, force, log_message);
}
