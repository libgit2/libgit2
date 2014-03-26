extern void rewrite_gitmodules(const char *workdir);

/* these will automatically set a cleanup callback */
extern git_repository *setup_fixture_submodules(void);
extern git_repository *setup_fixture_submod2(void);

extern unsigned int get_submodule_status(git_repository *, const char *);

extern void assert_submodule_exists(git_repository *, const char *);
extern void refute_submodule_exists(git_repository *, const char *, int err);
