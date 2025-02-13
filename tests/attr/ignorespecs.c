#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"
#include "fileops.h"

#include "common.h"

static git_repository *g_repo = NULL;

void test_attr_ignorespecs__initialize(void)
{
	g_repo = cl_git_sandbox_init_new("ignorespecs");
}

void test_attr_ignorespecs__cleanup(void)
{
	cl_git_sandbox_cleanup();
	g_repo = NULL;
}

static void setup_ignore_config(git_config *cfg_in)
{
	git_buf patterns = GIT_BUF_INIT;
	git_config *cfg;
	git_config_iterator *it;
	git_config_entry *entry;
	int error;

	cl_git_pass(git_config_snapshot(&cfg, cfg_in));

	cl_git_pass(git_config_iterator_glob_new(&it, cfg, "gitignore.*.file"));

	while (!(error = git_config_next(&entry, it))) {
		git_buf source = GIT_BUF_INIT, destination = GIT_BUF_INIT;
		const char *section_start, *section_end;

		/* Look up the path of the gitignore we want to write */
		cl_assert(section_start = strchr(entry->name, '.'));
		cl_assert(section_end = strrchr(entry->name, '.'));
		cl_git_pass(git_buf_PUTS(&destination, "ignorespecs/"));
		cl_git_pass(git_buf_put(&destination, section_start, section_end - section_start));

		/* Look up the path of the gitignore we want to read from */
		cl_git_pass(git_buf_printf(&source, "ignorespecs/%s", entry->value));

		cl_git_pass(git_futils_cp(cl_fixture(source.ptr), destination.ptr, 0644));

		git_buf_free(&destination);
		git_buf_free(&source);
	}

	cl_assert_equal_i(GIT_ITEROVER, error);

	git_config_iterator_free(it);
	git_config_free(cfg);
	git_buf_free(&patterns);
}

static void run_test_spec(const char *file)
{
	git_config_iterator *it;
	git_config_entry *entry;
	git_config *cfg;
	int error;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture(file)));
	setup_ignore_config(cfg);
	cl_git_pass(git_config_iterator_new(&it, cfg));

	while ((error = git_config_next(&entry, it)) == 0) {
		git_buf errmsg = GIT_BUF_INIT;
		int is_ignored = 0;

		cl_git_pass(git_ignore_path_is_ignored(&is_ignored, g_repo, entry->value));

		if (git__strcmp(entry->name, "assert.ignored") == 0 && !is_ignored) {
			git_buf_printf(&errmsg, "Expected file '%s' to be ignored", entry->value);
			cl_fail(errmsg.ptr);
		} else if (git__strcmp(entry->name, "assert.not-ignored") == 0 && is_ignored) {
			git_buf_printf(&errmsg, "Expected file '%s' to not be ignored", entry->value);
			cl_fail(errmsg.ptr);
		}

		git_buf_free(&errmsg);
	}
	cl_assert_equal_i(error, GIT_ITEROVER);

	git_config_iterator_free(it);
	git_config_free(cfg);
}

void test_attr_ignorespecs__honor_temporary_rules(void)
{
	run_test_spec("attr/ignores/temporary.conf");
}
