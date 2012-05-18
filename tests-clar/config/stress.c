#include "clar_libgit2.h"

#include "filebuf.h"
#include "fileops.h"
#include "posix.h"

void test_config_stress__initialize(void)
{
	git_filebuf file = GIT_FILEBUF_INIT;

	cl_git_pass(git_filebuf_open(&file, "git-test-config", 0));

	git_filebuf_printf(&file, "[color]\n\tui = auto\n");
	git_filebuf_printf(&file, "[core]\n\teditor = \n");

	cl_git_pass(git_filebuf_commit(&file, 0666));
}

void test_config_stress__cleanup(void)
{
	p_unlink("git-test-config");
}

void test_config_stress__dont_break_on_invalid_input(void)
{
	const char *editor, *color;
	struct git_config_file *file;
	git_config *config;

	cl_assert(git_path_exists("git-test-config"));
	cl_git_pass(git_config_file__ondisk(&file, "git-test-config"));
	cl_git_pass(git_config_new(&config));
	cl_git_pass(git_config_add_file(config, file, 0));

	cl_git_pass(git_config_get_string(&color, config, "color.ui"));
	cl_git_pass(git_config_get_string(&editor, config, "core.editor"));

	git_config_free(config);
}

void test_config_stress__comments(void)
{
	struct git_config_file *file;
	git_config *config;
	const char *str;

	cl_git_pass(git_config_file__ondisk(&file, cl_fixture("config/config12")));
	cl_git_pass(git_config_new(&config));
	cl_git_pass(git_config_add_file(config, file, 0));

	cl_git_pass(git_config_get_string(&str, config, "some.section.other"));
	cl_assert(!strcmp(str, "hello! \" ; ; ; "));

	cl_git_pass(git_config_get_string(&str, config, "some.section.multi"));
	cl_assert(!strcmp(str, "hi, this is a ; multiline comment # with ;\n special chars and other stuff !@#"));

	cl_git_pass(git_config_get_string(&str, config, "some.section.back"));
	cl_assert(!strcmp(str, "this is \ba phrase"));

	git_config_free(config);
}
