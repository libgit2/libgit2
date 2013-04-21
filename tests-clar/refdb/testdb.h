#include <git2/errors.h>
#include <git2/repository.h>
#include <git2/refdb.h>
#include <git2/sys/refs.h>
#include <git2/sys/refdb_backend.h>

int refdb_backend_test(
	git_refdb_backend **backend_out,
	git_repository *repo);
