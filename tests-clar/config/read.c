#include "clar_libgit2.h"

void test_config_read__simple_read(void)
{
	git_config *cfg;
	int32_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config0")));

	cl_git_pass(git_config_get_int32(cfg, "core.repositoryformatversion", &i));
	cl_assert(i == 0);
	cl_git_pass(git_config_get_bool(cfg, "core.filemode", &i));
	cl_assert(i == 1);
	cl_git_pass(git_config_get_bool(cfg, "core.bare", &i));
	cl_assert(i == 0);
	cl_git_pass(git_config_get_bool(cfg, "core.logallrefupdates", &i));
	cl_assert(i == 1);

	git_config_free(cfg);
}

void test_config_read__case_sensitive(void)
{
	git_config *cfg;
	int i;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config1")));

	cl_git_pass(git_config_get_string(cfg, "this.that.other", &str));
	cl_assert(!strcmp(str, "true"));
	cl_git_pass(git_config_get_string(cfg, "this.That.other", &str));
	cl_assert(!strcmp(str, "yes"));

	cl_git_pass(git_config_get_bool(cfg, "this.that.other", &i));
	cl_assert(i == 1);
	cl_git_pass(git_config_get_bool(cfg, "this.That.other", &i));
	cl_assert(i == 1);

	/* This one doesn't exist */
	cl_must_fail(git_config_get_bool(cfg, "this.thaT.other", &i));

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

	cl_git_pass(git_config_get_string(cfg, "this.That.and", &str));
	cl_assert(!strcmp(str, "one one one two two three three"));

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

	cl_git_pass(git_config_get_string(cfg, "section.subsection.var", &str));
	cl_assert(!strcmp(str, "hello"));

	/* The subsection is transformed to lower-case */
	cl_must_fail(git_config_get_string(cfg, "section.subSectIon.var", &str));

	git_config_free(cfg);
}

void test_config_read__lone_variable(void)
{
	git_config *cfg;
	const char *str;
	int i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config4")));

	cl_git_pass(git_config_get_string(cfg, "some.section.variable", &str));
	cl_assert(str == NULL);

	cl_git_pass(git_config_get_bool(cfg, "some.section.variable", &i));
	cl_assert(i == 1);

	git_config_free(cfg);
}

void test_config_read__number_suffixes(void)
{
	git_config *cfg;
	int64_t i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config5")));

	cl_git_pass(git_config_get_int64(cfg, "number.simple", &i));
	cl_assert(i == 1);

	cl_git_pass(git_config_get_int64(cfg, "number.k", &i));
	cl_assert(i == 1 * 1024);

	cl_git_pass(git_config_get_int64(cfg, "number.kk", &i));
	cl_assert(i == 1 * 1024);

	cl_git_pass(git_config_get_int64(cfg, "number.m", &i));
	cl_assert(i == 1 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(cfg, "number.mm", &i));
	cl_assert(i == 1 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(cfg, "number.g", &i));
	cl_assert(i == 1 * 1024 * 1024 * 1024);

	cl_git_pass(git_config_get_int64(cfg, "number.gg", &i));
	cl_assert(i == 1 * 1024 * 1024 * 1024);

	git_config_free(cfg);
}

void test_config_read__blank_lines(void)
{
	git_config *cfg;
	int i;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config6")));

	cl_git_pass(git_config_get_bool(cfg, "valid.subsection.something", &i));
	cl_assert(i == 1);

	cl_git_pass(git_config_get_bool(cfg, "something.else.something", &i));
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
	cl_git_pass(git_config_get_string(cfg, "remote.ab.url", &str));
	cl_assert(strcmp(str, "http://example.com/git/ab") == 0);

	cl_git_pass(git_config_get_string(cfg, "remote.abba.url", &str));
	cl_assert(strcmp(str, "http://example.com/git/abba") == 0);

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
