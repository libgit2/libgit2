#include "clar_libgit2.h"

#include "repository.h"
#include "reflog.h"

size_t reflog_entrycount(git_repository *repo, const char *name)
{
	git_reflog *log;
	size_t ret;

	cl_git_pass(git_reflog_read(&log, repo, name));
	ret = git_reflog_entrycount(log);
	git_reflog_free(log);

	return ret;
}

void reflog_check_entry(git_repository *repo, const char *reflog, size_t idx,
	const char *old_spec, const char *new_spec,
	const char *email, const char *message)
{
	git_reflog *log;
	const git_reflog_entry *entry;

	cl_git_pass(git_reflog_read(&log, repo, reflog));
	entry = git_reflog_entry_byindex(log, idx);

	if (old_spec) {
		git_object *obj = NULL;
		if (git_revparse_single(&obj, repo, old_spec) == GIT_OK) {
			cl_assert_equal_oid(git_object_id(obj), git_reflog_entry_id_old(entry));
			git_object_free(obj);
		} else {
			git_oid *oid = git__calloc(1, sizeof(*oid));
			git_oid_fromstr(oid, old_spec);
			cl_assert_equal_oid(oid, git_reflog_entry_id_old(entry));
			git__free(oid);
		}
	}
	if (new_spec) {
		git_object *obj = NULL;
		if (git_revparse_single(&obj, repo, new_spec) == GIT_OK) {
			cl_assert_equal_oid(git_object_id(obj), git_reflog_entry_id_new(entry));
			git_object_free(obj);
		} else {
			git_oid *oid = git__calloc(1, sizeof(*oid));
			git_oid_fromstr(oid, new_spec);
			cl_assert_equal_oid(oid, git_reflog_entry_id_new(entry));
			git__free(oid);
		}
	}

	if (email) {
		cl_assert_equal_s(email, git_reflog_entry_committer(entry)->email);
	}
	if (message) {
		cl_assert_equal_s(message, git_reflog_entry_message(entry));
	}

	git_reflog_free(log);
}

void reflog_print(git_repository *repo, const char *reflog_name)
{
	git_reflog *reflog;
	size_t idx;
	git_buf out = GIT_BUF_INIT;

	git_reflog_read(&reflog, repo, reflog_name);

	for (idx = 0; idx < git_reflog_entrycount(reflog); idx++) {
		const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, idx);
		char old_oid[GIT_OID_HEXSZ], new_oid[GIT_OID_HEXSZ];;

		git_oid_tostr((char *)&old_oid, GIT_OID_HEXSZ, git_reflog_entry_id_old(entry));
		git_oid_tostr((char *)&new_oid, GIT_OID_HEXSZ, git_reflog_entry_id_new(entry));

		git_buf_printf(&out, "%ld: %s %s %s %s\n", idx, old_oid, new_oid, "somesig", git_reflog_entry_message(entry));
	}

	fprintf(stderr, "%s", git_buf_cstr(&out));
	git_buf_free(&out);
	git_reflog_free(reflog);
}
