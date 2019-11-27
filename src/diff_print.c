#include "futils.h"
		else if (git_repository__configmap_lookup(&pi->id_strlen, repo, GIT_CONFIGMAP_ABBREV) < 0)
	git_buf *out, const git_diff_delta *delta, int id_strlen,
	bool print_index)
		if (print_index)
			git_buf_printf(out, "index %s..%s %o\n",
				start_oid, end_oid, delta->old_file.mode);
		if (print_index)
			git_buf_printf(out, "index %s..%s\n", start_oid, end_oid);
	if (git_oid_is_zero(&delta->old_file.id))
	if (git_oid_is_zero(&delta->new_file.id))
	if (git_oid_is_zero(&delta->old_file.id) &&
		git_oid_is_zero(&delta->new_file.id))
	int id_strlen,
	bool print_index)
		if ((error = diff_print_oid_range(out, delta,
						  id_strlen, print_index)) < 0)
	bool print_index = (pi->format != GIT_DIFF_FORMAT_PATCH_ID);
			pi->buf, delta, oldpfx, newpfx,
			id_strlen, print_index)) < 0)
	case GIT_DIFF_FORMAT_PATCH_ID:
		print_file = diff_print_patch_file;
		print_binary = diff_print_patch_binary;
		print_line = diff_print_patch_line;
		break;