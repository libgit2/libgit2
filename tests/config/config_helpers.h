extern void assert_config_entry_existence(
	git_repository *repo,
	const char *name,
	bool is_supposed_to_exist);

extern void assert_config_entry_value(
	git_repository *repo,
	const char *name,
	const char *expected_value);
