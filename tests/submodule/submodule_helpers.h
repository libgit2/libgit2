extern void rewrite_gitmodules(const char *workdir);

/* these will automatically set a cleanup callback */
extern git_repository *setup_fixture_submodules(void);
extern git_repository *setup_fixture_submod2(void);
