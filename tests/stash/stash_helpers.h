void setup_stash(
	git_repository *repo,
	git_signature *signature);

void assert_status(
	git_repository *repo,
	const char *path,
	int status_flags);

void assert_object_oid(
	git_repository *repo,
	const char* revision,
	const char* expected_oid,
	git_object_t type);
