#include "fileops.h"
		else if (git_repository__cvar(&pi->id_strlen, repo, GIT_CVAR_ABBREV) < 0)
	git_buf *out, const git_diff_delta *delta, int id_strlen)
		git_buf_printf(out, "index %s..%s %o\n",
			start_oid, end_oid, delta->old_file.mode);
		git_buf_printf(out, "index %s..%s\n", start_oid, end_oid);
	if (git_oid_iszero(&delta->old_file.id))
	if (git_oid_iszero(&delta->new_file.id))
	if (git_oid_iszero(&delta->old_file.id) &&
		git_oid_iszero(&delta->new_file.id))
	int id_strlen)
		if ((error = diff_print_oid_range(out, delta, id_strlen)) < 0)
			pi->buf, delta, oldpfx, newpfx, id_strlen)) < 0)