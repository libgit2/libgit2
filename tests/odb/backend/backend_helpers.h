#include "git2/sys/odb_backend.h"

typedef struct {
	git_odb_backend parent;

	git_error_code error_code;
	git_oid oid;

	int exists_calls;
	int read_calls;
	int read_header_calls;
	int read_prefix_calls;
} fake_backend;

int build_fake_backend(
	git_odb_backend **out,
	git_error_code error_code,
	const git_oid *oid);
