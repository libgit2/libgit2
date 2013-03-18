#include "clar_libgit2.h"

void test_config_write__initialize(void)
{
	cl_fixture_sandbox("config/config9");
	cl_fixture_sandbox("config/config15");
	cl_fixture_sandbox("config/config17");
}

void test_config_write__cleanup(void)
{
	cl_fixture_cleanup("config9");
	cl_fixture_cleanup("config15");
	cl_fixture_cleanup("config17");
}

void test_config_write__replace_value(void)
{
	git_config *cfg;
	int i;
	int64_t l, expected = +9223372036854775803;

	/* By freeing the config, we make sure we flush the values  */
	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_int32(cfg, "core.dummy", 5));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_int32(&i, cfg, "core.dummy"));
	cl_assert(i == 5);
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_int32(cfg, "core.dummy", 1));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_int64(cfg, "core.verylong", expected));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_int64(&l, cfg, "core.verylong"));
	cl_assert(l == expected);
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_must_fail(git_config_get_int32(&i, cfg, "core.verylong"));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_int64(cfg, "core.verylong", 1));
	git_config_free(cfg);
}

void test_config_write__delete_value(void)
{
	git_config *cfg;
	int32_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_int32(cfg, "core.dummy", 5));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_delete_entry(cfg, "core.dummy"));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_assert(git_config_get_int32(&i, cfg, "core.dummy") == GIT_ENOTFOUND);
	cl_git_pass(git_config_set_int32(cfg, "core.dummy", 1));
	git_config_free(cfg);
}

/*
 * At the beginning of the test:
 *  - config9 has: core.dummy2=42
 *  - config15 has: core.dummy2=7
 */
void test_config_write__delete_value_at_specific_level(void)
{
	git_config *cfg, *cfg_specific;
	int32_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, "config15"));
	cl_git_pass(git_config_get_int32(&i, cfg, "core.dummy2"));
	cl_assert(i == 7);
	git_config_free(cfg);

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, "config9",
		GIT_CONFIG_LEVEL_LOCAL, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, "config15",
		GIT_CONFIG_LEVEL_GLOBAL, 0));

	cl_git_pass(git_config_open_level(&cfg_specific, cfg, GIT_CONFIG_LEVEL_GLOBAL));

	cl_git_pass(git_config_delete_entry(cfg_specific, "core.dummy2"));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config15"));
	cl_assert(git_config_get_int32(&i, cfg, "core.dummy2") == GIT_ENOTFOUND);
	cl_git_pass(git_config_set_int32(cfg, "core.dummy2", 7));

	git_config_free(cfg_specific);
	git_config_free(cfg);
}

void test_config_write__write_subsection(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_string(cfg, "my.own.var", "works"));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_string(&str, cfg, "my.own.var"));
	cl_assert_equal_s("works", str);
	git_config_free(cfg);
}

void test_config_write__delete_inexistent(void)
{
	git_config *cfg;

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_assert(git_config_delete_entry(cfg, "core.imaginary") == GIT_ENOTFOUND);
	git_config_free(cfg);
}

void test_config_write__value_containing_quotes(void)
{
	git_config *cfg;
	const char* str;

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_string(cfg, "core.somevar", "this \"has\" quotes"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this \"has\" quotes");
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this \"has\" quotes");
	git_config_free(cfg);

	/* The code path for values that already exist is different, check that one as well */
	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_string(cfg, "core.somevar", "this also \"has\" quotes"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this also \"has\" quotes");
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this also \"has\" quotes");
	git_config_free(cfg);
}

void test_config_write__escape_value(void)
{
	git_config *cfg;
	const char* str;

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_set_string(cfg, "core.somevar", "this \"has\" quotes and \t"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this \"has\" quotes and \t");
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config9"));
	cl_git_pass(git_config_get_string(&str, cfg, "core.somevar"));
	cl_assert_equal_s(str, "this \"has\" quotes and \t");
	git_config_free(cfg);
}

void test_config_write__add_value_at_specific_level(void)
{
	git_config *cfg, *cfg_specific;
	int i;
	int64_t l, expected = +9223372036854775803;
	const char *s;

	// open config15 as global level config file
	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, "config9",
		GIT_CONFIG_LEVEL_LOCAL, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, "config15",
		GIT_CONFIG_LEVEL_GLOBAL, 0));

	cl_git_pass(git_config_open_level(&cfg_specific, cfg, GIT_CONFIG_LEVEL_GLOBAL));

	cl_git_pass(git_config_set_int32(cfg_specific, "core.int32global", 28));
	cl_git_pass(git_config_set_int64(cfg_specific, "core.int64global", expected));
	cl_git_pass(git_config_set_bool(cfg_specific, "core.boolglobal", true));
	cl_git_pass(git_config_set_string(cfg_specific, "core.stringglobal", "I'm a global config value!"));
	git_config_free(cfg_specific);
	git_config_free(cfg);

	// open config15 as local level config file
	cl_git_pass(git_config_open_ondisk(&cfg, "config15"));

	cl_git_pass(git_config_get_int32(&i, cfg, "core.int32global"));
	cl_assert_equal_i(28, i);
	cl_git_pass(git_config_get_int64(&l, cfg, "core.int64global"));
	cl_assert(l == expected);
	cl_git_pass(git_config_get_bool(&i, cfg, "core.boolglobal"));
	cl_assert_equal_b(true, i);
	cl_git_pass(git_config_get_string(&s, cfg, "core.stringglobal"));
	cl_assert_equal_s("I'm a global config value!", s);

	git_config_free(cfg);
}

void test_config_write__add_value_at_file_with_no_clrf_at_the_end(void)
{
	git_config *cfg;
	int i;

	cl_git_pass(git_config_open_ondisk(&cfg, "config17"));
	cl_git_pass(git_config_set_int32(cfg, "core.newline", 7));
	git_config_free(cfg);

	cl_git_pass(git_config_open_ondisk(&cfg, "config17"));
	cl_git_pass(git_config_get_int32(&i, cfg, "core.newline"));
	cl_assert_equal_i(7, i);

	git_config_free(cfg);
}

void test_config_write__can_set_a_value_to_NULL(void)
{
    git_repository *repository;
    git_config *config;

    repository = cl_git_sandbox_init("testrepo.git");

    cl_git_pass(git_repository_config(&config, repository));
    cl_git_fail(git_config_set_string(config, "a.b.c", NULL));
    git_config_free(config);

    cl_git_sandbox_cleanup();
}
