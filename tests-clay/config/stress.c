#include "clay_libgit2.h"

#include "filebuf.h"
#include "fileops.h"
#include "posix.h"

#define TEST_CONFIG "git-test-config"

void test_config_stress__initialize(void)
{
	git_filebuf file;

	git_filebuf_open(&file, TEST_CONFIG, 0);

	git_filebuf_printf(&file, "[color]\n\tui = auto\n");
	git_filebuf_printf(&file, "[core]\n\teditor = \n");

	git_filebuf_commit(&file, 0666);
}

void test_config_stress__cleanup(void)
{
	p_unlink(TEST_CONFIG);
}

void test_config_stress__dont_break_on_invalid_input(void)
{
	const char *editor, *color;
	struct git_config_file *file;
	git_config *config;

	cl_git_pass(git_futils_exists(TEST_CONFIG));
	cl_git_pass(git_config_file__ondisk(&file, TEST_CONFIG));
	cl_git_pass(git_config_new(&config));
	cl_git_pass(git_config_add_file(config, file, 0));

	cl_git_pass(git_config_get_string(config, "color.ui", &color));
	cl_git_pass(git_config_get_string(config, "core.editor", &editor));

	git_config_free(config);
}
