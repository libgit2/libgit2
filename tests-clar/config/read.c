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

static int count_cfg_entries_and_compare_levels(
	const git_config_entry *entry, void *payload)
{
	int *count = payload;

	if (!strcmp(entry->value, "7") || !strcmp(entry->value, "17"))
		cl_assert(entry->level == GIT_CONFIG_LEVEL_GLOBAL);
	else
		cl_assert(entry->level == GIT_CONFIG_LEVEL_SYSTEM);

	(*count)++;
	return 0;
}

static int cfg_callback_countdown(const git_config_entry *entry, void *payload)
{
	int *count = payload;
	GIT_UNUSED(entry);
	(*count)--;
	if (*count == 0)
		return -100;
	return 0;
}

void test_config_read__foreach(void)
{
	git_config *cfg;
	int count, ret;

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config9"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config15"),
		GIT_CONFIG_LEVEL_GLOBAL, 0));

	count = 0;
	cl_git_pass(git_config_foreach(cfg, count_cfg_entries_and_compare_levels, &count));
	cl_assert_equal_i(7, count);

	count = 3;
	cl_git_fail(ret = git_config_foreach(cfg, cfg_callback_countdown, &count));
	cl_assert_equal_i(GIT_EUSER, ret);

	git_config_free(cfg);
}

static int count_cfg_entries(const git_config_entry *entry, void *payload)
{
	int *count = payload;
	GIT_UNUSED(entry);
	(*count)++;
	return 0;
}

void test_config_read__foreach_match(void)
{
	git_config *cfg;
	int count;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config9")));

	count = 0;
	cl_git_pass(
		git_config_foreach_match(cfg, "core.*", count_cfg_entries, &count));
	cl_assert_equal_i(3, count);

	count = 0;
	cl_git_pass(
		git_config_foreach_match(cfg, "remote\\.ab.*", count_cfg_entries, &count));
	cl_assert_equal_i(2, count);

	count = 0;
	cl_git_pass(
		git_config_foreach_match(cfg, ".*url$", count_cfg_entries, &count));
	cl_assert_equal_i(2, count);

	count = 0;
	cl_git_pass(
		git_config_foreach_match(cfg, ".*dummy.*", count_cfg_entries, &count));
	cl_assert_equal_i(2, count);

	count = 0;
	cl_git_pass(
		git_config_foreach_match(cfg, ".*nomatch.*", count_cfg_entries, &count));
	cl_assert_equal_i(0, count);

	git_config_free(cfg);
}

void test_config_read__whitespace_not_required_around_assignment(void)
{
	git_config *cfg;
	const char *str;

	cl_git_pass(git_config_open_ondisk(&cfg, cl_fixture("config/config14")));

	cl_git_pass(git_config_get_string(&str, cfg, "a.b"));
	cl_assert_equal_s(str, "c");

	cl_git_pass(git_config_get_string(&str, cfg, "d.e"));
	cl_assert_equal_s(str, "f");

	git_config_free(cfg);
}

void test_config_read__read_git_config_entry(void)
{
	git_config *cfg;
	const git_config_entry *entry;

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config9"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));

	cl_git_pass(git_config_get_config_entry(&entry, cfg, "core.dummy2"));
	cl_assert_equal_s("core.dummy2", entry->name);
	cl_assert_equal_s("42", entry->value);
	cl_assert_equal_i(GIT_CONFIG_LEVEL_SYSTEM, entry->level);

	git_config_free(cfg);
}

/*
 * At the beginning of the test:
 *  - config9 has: core.dummy2=42
 *  - config15 has: core.dummy2=7
 *  - config16 has: core.dummy2=28
 */
void test_config_read__local_config_overrides_global_config_overrides_system_config(void)
{
	git_config *cfg;
	int32_t i;

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config9"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config15"),
		GIT_CONFIG_LEVEL_GLOBAL, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config16"),
		GIT_CONFIG_LEVEL_LOCAL, 0));

	cl_git_pass(git_config_get_int32(&i, cfg, "core.dummy2"));
	cl_assert_equal_i(28, i);

	git_config_free(cfg);

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config9"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config15"),
		GIT_CONFIG_LEVEL_GLOBAL, 0));

	cl_git_pass(git_config_get_int32(&i, cfg, "core.dummy2"));
	cl_assert_equal_i(7, i);

	git_config_free(cfg);
}

/*
 * At the beginning of the test:
 *  - config9 has: core.global does not exist
 *  - config15 has: core.global=17
 *  - config16 has: core.global=29
 *
 * And also:
 *  - config9 has: core.system does not exist
 *  - config15 has: core.system does not exist
 *  - config16 has: core.system=11
 */
void test_config_read__fallback_from_local_to_global_and_from_global_to_system(void)
{
	git_config *cfg;
	int32_t i;

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config9"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config15"),
		GIT_CONFIG_LEVEL_GLOBAL, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config16"),
		GIT_CONFIG_LEVEL_LOCAL, 0));

	cl_git_pass(git_config_get_int32(&i, cfg, "core.global"));
	cl_assert_equal_i(17, i);
	cl_git_pass(git_config_get_int32(&i, cfg, "core.system"));
	cl_assert_equal_i(11, i);

	git_config_free(cfg);
}

/*
 * At the beginning of the test, config18 has:
 *	int32global = 28
 *	int64global = 9223372036854775803
 *	boolglobal = true
 *	stringglobal = I'm a global config value!
 *
 * And config19 has:
 *	int32global = -1
 *	int64global = -2
 *	boolglobal = false
 *	stringglobal = don't find me!
 *
 */
void test_config_read__simple_read_from_specific_level(void)
{
	git_config *cfg, *cfg_specific;
	int i;
	int64_t l, expected = +9223372036854775803;
	const char *s;

	cl_git_pass(git_config_new(&cfg));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config18"),
		GIT_CONFIG_LEVEL_GLOBAL, 0));
	cl_git_pass(git_config_add_file_ondisk(cfg, cl_fixture("config/config19"),
		GIT_CONFIG_LEVEL_SYSTEM, 0));

	cl_git_pass(git_config_open_level(&cfg_specific, cfg, GIT_CONFIG_LEVEL_GLOBAL));

	cl_git_pass(git_config_get_int32(&i, cfg_specific, "core.int32global"));
	cl_assert_equal_i(28, i);
	cl_git_pass(git_config_get_int64(&l, cfg_specific, "core.int64global"));
	cl_assert(l == expected);
	cl_git_pass(git_config_get_bool(&i, cfg_specific, "core.boolglobal"));
	cl_assert_equal_b(true, i);
	cl_git_pass(git_config_get_string(&s, cfg_specific, "core.stringglobal"));
	cl_assert_equal_s("I'm a global config value!", s);

	git_config_free(cfg_specific);
	git_config_free(cfg);
}
