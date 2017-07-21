size_t reflog_entrycount(git_repository *repo, const char *name);
void reflog_check_entry(git_repository *repo, const char *reflog, size_t idx,
						const char *old_spec, const char *new_spec,
						const char *email, const char *message);

void reflog_print(git_repository *repo, const char *reflog_name);
