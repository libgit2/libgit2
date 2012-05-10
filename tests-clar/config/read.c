#include "clar_libgit2.h"

void test_config_read__simple_read(void)
{
	git_config *cfg;
	int32_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config0")));

	cl_git_pass(git_config_get_int32(&i, cfg, "core.repositoryformatversion"));
	cl_assert(i == 0);
	cl_git_pass(git_config_get_bool(&i, cfg, "core.filemode"));
	cl_assert(i == 1);
	cl_git_pass(git_config_get_bool(&i, cfg, "core.bare"));
	cl_assert(i == 0);
	cl_git_pass(git_config_get_bool(&i, cfg, "core.logallrefupdates"));
	cl_assert(i == 1);

	git_config_free(cfg);
}

void test_config_read__case_sensitive(void)
{
	git_config *cfg;
	int i;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config1")));

	cl_git_pass(git_config_get_string(&str, cfg, "this.that.other"));
	cl_assert_equal_s(str, "true");
	cl_git_pass(git_config_get_string(&str, cfg, "this.That.other"));
	cl_assert_equal_s(str, "yes");

	cl_git_pass(git_config_get_bool(&i, cfg, "this.that.other"));
	cl_assert(i == 1);
	cl_git_pass(git_config_get_bool(&i, cfg, "this.That.other"));
	cl_assert(i == 1);

	/* This one doesn't exist */
	cl_must_fail(git_config_get_bool(&i, cfg, "this.thaT.other"));

	git_config_free(cfg);
}

/*
 * If \ is the last non-space character on the line, we read the next
 * one, separating each line with SP.
 */
void test_config_read__multiline_value(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config2")));

	cl_git_pass(git_config_get_string(&str, cfg, "this.That.and"));
	cl_assert_equal_s(str, "one one one two two three three");

	git_config_free(cfg);
}

/*
 * This kind of subsection declaration is case-insensitive
 */
void test_config_read__subsection_header(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config3")));

	cl_git_pass(git_config_get_string(&str, cfg, "section.subsection.var"));
	cl_assert_equal_s(str, "hello");

	/* The subsection is transformed to lower-case */
	cl_must_fail(git_config_get_string(&str, cfg, "section.subSectIon.var"));

	git_config_free(cfg);
}

void test_config_read__lone_variable(void)
{
	git_config *cfg;
	const char *str;
	int i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config4")));

	cl_git_pass(git_config_get_string(&str, cfg, "some.section.variable"));
	cl_assert(str == NULL);

	cl_git_pass(git_config_get_bool(&i, cfg, "some.section.variable"));
	cl_assert(i == 1);

	git_config_free(cfg);
}

void test_config_read__number_suffixes(void)
{
	git_config *cfg;
	int64_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config5")));

	cl_git_pass(git_config_get_int64(&i, cfg, "number.simple"));
	cl_assert(i == 1);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.k"));
	cl_assert(i == 1 * 1024);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.kk"));
	cl_assert(i == 1 * 1024);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.m"));
	cl_assert(i == 1 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.mm"));
	cl_assert(i == 1 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.g"));
	cl_assert(i == 1 * 1024 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(&i, cfg, "number.gg"));
	cl_assert(i == 1 * 1024 * 1024 * 1024);

	git_config_free(cfg);
}

void test_config_read__blank_lines(void)
{
	git_config *cfg;
	int i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config6")));

	cl_git_pass(git_config_get_bool(&i, cfg, "valid.subsection.something"));
	cl_assert(i == 1);

	cl_git_pass(git_config_get_bool(&i, cfg, "something.else.something"));
	cl_assert(i == 0);

	git_config_free(cfg);
}

void test_config_read__invalid_ext_headers(void)
{
	git_config *cfg;
	cl_must_fail(git_config_open_ondisk(&cfg, cl_fixture("config/config7")));
}

void test_config_read__empty_files(void)
{
	git_config *cfg;
	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config8")));
	git_config_free(cfg);
}

void test_config_read__header_in_last_line(void)
{
	git_config *cfg;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config10")));
	git_config_free(cfg);
}

void test_config_read__prefixes(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config9")));
	cl_git_pass(git_config_get_string(&str, cfg, "remote.ab.url"));
	cl_assert_equal_s(str, "http://example.com/git/ab");

	cl_git_pass(git_config_get_string(&str, cfg, "remote.abba.url"));
	cl_assert_equal_s(str, "http://example.com/git/abba");

	git_config_free(cfg);
}

void test_config_read__escaping_quotes(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config13")));
	cl_git_pass(git_config_get_string(&str, cfg, "core.editor"));
	cl_assert(strcmp(str, "\"C:/Program Files/Nonsense/bah.exe\" \"--some option\"") == 0);

	git_config_free(cfg);
}

#if 0

BEGIN_TEST(config10, "a repo's config overrides the global config")
	git_repository *repo;
	git_config *cfg;
	int32_t version;

	cl_git_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	cl_git_pass(git_repository_config(&cfg, repo, GLOBAL_CONFIG, NULL));
	cl_git_pass(git_config_get_int32(cfg, "core.repositoryformatversion", &version));
	cl_assert(version == 0);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(config11, "fall back to the global config")
	git_repository *repo;
	git_config *cfg;
	int32_t num;

	cl_git_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	cl_git_pass(git_repository_config(&cfg, repo, GLOBAL_CONFIG, NULL));
	cl_git_pass(git_config_get_int32(cfg, "core.something", &num));
	cl_assert(num == 2);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST
#endif
