void setup_stash(
	git_repository *repo,
	git_signature *signature);

void commit_staged_files(
	git_oid *commit_oid,
	git_index *index,
	git_signature *signature);