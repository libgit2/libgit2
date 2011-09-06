/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include <git2.h>
#include <posix.h>
#include "filebuf.h"

#define CONFIG_BASE TEST_RESOURCES "/config"
#define GLOBAL_CONFIG CONFIG_BASE "/.gitconfig"

/*
 * This one is so we know the code isn't completely broken
 */
BEGIN_TEST(config0, "read a simple configuration")
	git_config *cfg;
	int32_t i;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config0"));
	must_pass(git_config_get_int32(cfg, "core.repositoryformatversion", &i));
	must_be_true(i == 0);
	must_pass(git_config_get_bool(cfg, "core.filemode", &i));
	must_be_true(i == 1);
	must_pass(git_config_get_bool(cfg, "core.bare", &i));
	must_be_true(i == 0);
	must_pass(git_config_get_bool(cfg, "core.logallrefupdates", &i));
	must_be_true(i == 1);

	git_config_free(cfg);
END_TEST

/*
 * [this "that"] and [this "That] are different namespaces. Make sure
 * each returns the correct one.
 */
BEGIN_TEST(config1, "case sensitivity")
	git_config *cfg;
	int i;
	const char *str;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config1"));

	must_pass(git_config_get_string(cfg, "this.that.other", &str));
	must_be_true(!strcmp(str, "true"));
	must_pass(git_config_get_string(cfg, "this.That.other", &str));
	must_be_true(!strcmp(str, "yes"));

	must_pass(git_config_get_bool(cfg, "this.that.other", &i));
	must_be_true(i == 1);
	must_pass(git_config_get_bool(cfg, "this.That.other", &i));
	must_be_true(i == 1);

	/* This one doesn't exist */
	must_fail(git_config_get_bool(cfg, "this.thaT.other", &i));

	git_config_free(cfg);
END_TEST

/*
 * If \ is the last non-space character on the line, we read the next
 * one, separating each line with SP.
 */
BEGIN_TEST(config2, "parse a multiline value")
	git_config *cfg;
	const char *str;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config2"));

	must_pass(git_config_get_string(cfg, "this.That.and", &str));
	must_be_true(!strcmp(str, "one one one two two three three"));

	git_config_free(cfg);
END_TEST

/*
 * This kind of subsection declaration is case-insensitive
 */
BEGIN_TEST(config3, "parse a [section.subsection] header")
	git_config *cfg;
	const char *str;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config3"));

	must_pass(git_config_get_string(cfg, "section.subsection.var", &str));
	must_be_true(!strcmp(str, "hello"));

	/* The subsection is transformed to lower-case */
	must_fail(git_config_get_string(cfg, "section.subSectIon.var", &str));

	git_config_free(cfg);
END_TEST

BEGIN_TEST(config4, "a variable name on its own is valid")
	git_config *cfg;
const char *str;
int i;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config4"));

	must_pass(git_config_get_string(cfg, "some.section.variable", &str));
	must_be_true(str == NULL);

	must_pass(git_config_get_bool(cfg, "some.section.variable", &i));
	must_be_true(i == 1);


	git_config_free(cfg);
END_TEST

BEGIN_TEST(config5, "test number suffixes")
	git_config *cfg;
	int64_t i;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config5"));

	must_pass(git_config_get_int64(cfg, "number.simple", &i));
	must_be_true(i == 1);

	must_pass(git_config_get_int64(cfg, "number.k", &i));
	must_be_true(i == 1 * 1024);

	must_pass(git_config_get_int64(cfg, "number.kk", &i));
	must_be_true(i == 1 * 1024);

	must_pass(git_config_get_int64(cfg, "number.m", &i));
	must_be_true(i == 1 * 1024 * 1024);

	must_pass(git_config_get_int64(cfg, "number.mm", &i));
	must_be_true(i == 1 * 1024 * 1024);

	must_pass(git_config_get_int64(cfg, "number.g", &i));
	must_be_true(i == 1 * 1024 * 1024 * 1024);

	must_pass(git_config_get_int64(cfg, "number.gg", &i));
	must_be_true(i == 1 * 1024 * 1024 * 1024);

	git_config_free(cfg);
END_TEST

BEGIN_TEST(config6, "test blank lines")
	git_config *cfg;
	int i;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config6"));

	must_pass(git_config_get_bool(cfg, "valid.subsection.something", &i));
	must_be_true(i == 1);

	must_pass(git_config_get_bool(cfg, "something.else.something", &i));
	must_be_true(i == 0);

	git_config_free(cfg);
END_TEST

BEGIN_TEST(config7, "test for invalid ext headers")
	git_config *cfg;

	must_fail(git_config_open_ondisk(&cfg, CONFIG_BASE "/config7"));

END_TEST

BEGIN_TEST(config8, "don't fail on empty files")
	git_config *cfg;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config8"));

	git_config_free(cfg);
END_TEST

BEGIN_TEST(config9, "replace a value")
	git_config *cfg;
	int i;
	int64_t l, expected = +9223372036854775803;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_set_int32(cfg, "core.dummy", 5));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_get_int32(cfg, "core.dummy", &i));
	must_be_true(i == 5);
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_set_int32(cfg, "core.dummy", 1));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_set_int64(cfg, "core.verylong", expected));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_get_int64(cfg, "core.verylong", &l));
	must_be_true(l == expected);
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_fail(git_config_get_int32(cfg, "core.verylong", &i));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_set_int64(cfg, "core.verylong", 1));
	git_config_free(cfg);

END_TEST

BEGIN_TEST(config10, "a repo's config overrides the global config")
	git_repository *repo;
	git_config *cfg;
	int32_t version;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, GLOBAL_CONFIG, NULL));
	must_pass(git_config_get_int32(cfg, "core.repositoryformatversion", &version));
	must_be_true(version == 0);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(config11, "fall back to the global config")
	git_repository *repo;
	git_config *cfg;
	int32_t num;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, GLOBAL_CONFIG, NULL));
	must_pass(git_config_get_int32(cfg, "core.something", &num));
	must_be_true(num == 2);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(config12, "delete a value")
	git_config *cfg;
	int32_t i;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_set_int32(cfg, "core.dummy", 5));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_delete(cfg, "core.dummy"));
	git_config_free(cfg);

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_be_true(git_config_get_int32(cfg, "core.dummy", &i) == GIT_ENOTFOUND);
	must_pass(git_config_set_int32(cfg, "core.dummy", 1));
	git_config_free(cfg);
END_TEST

BEGIN_TEST(config13, "can't delete a non-existent value")
	git_config *cfg;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_be_true(git_config_delete(cfg, "core.imaginary") == GIT_ENOTFOUND);
	git_config_free(cfg);
END_TEST

BEGIN_TEST(config14, "don't fail horribly if a section header is in the last line")
	git_config *cfg;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config10"));
	git_config_free(cfg);
END_TEST

BEGIN_TEST(config15, "add a variable in an existing section")
	git_config *cfg;
	int32_t i;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config10"));
	must_pass(git_config_set_int32(cfg, "empty.tmp", 5));
	must_pass(git_config_get_int32(cfg, "empty.tmp", &i));
	must_be_true(i == 5);
	must_pass(git_config_delete(cfg, "empty.tmp"));
	git_config_free(cfg);
END_TEST

BEGIN_TEST(config16, "add a variable in a new section")
	git_config *cfg;
	int32_t i;
	git_filebuf buf;

	/* By freeing the config, we make sure we flush the values  */
	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config10"));
	must_pass(git_config_set_int32(cfg, "section.tmp", 5));
	must_pass(git_config_get_int32(cfg, "section.tmp", &i));
	must_be_true(i == 5);
	must_pass(git_config_delete(cfg, "section.tmp"));
	git_config_free(cfg);

	/* As the section wasn't removed, owerwrite the file */
	must_pass(git_filebuf_open(&buf, CONFIG_BASE "/config10", 0));
	must_pass(git_filebuf_write(&buf, "[empty]\n", strlen("[empty]\n")));
	must_pass(git_filebuf_commit(&buf, 0666));
END_TEST

BEGIN_TEST(config17, "prefixes aren't broken")
	git_config *cfg;
	const char *str;

	must_pass(git_config_open_ondisk(&cfg, CONFIG_BASE "/config9"));
	must_pass(git_config_get_string(cfg, "remote.ab.url", &str));
	must_be_true(strcmp(str, "http://example.com/git/ab") == 0);

	must_pass(git_config_get_string(cfg, "remote.abba.url", &str));
	must_be_true(strcmp(str, "http://example.com/git/abba") == 0);

	git_config_free(cfg);
END_TEST

BEGIN_SUITE(config)
	 ADD_TEST(config0);
	 ADD_TEST(config1);
	 ADD_TEST(config2);
	 ADD_TEST(config3);
	 ADD_TEST(config4);
	 ADD_TEST(config5);
	 ADD_TEST(config6);
	 ADD_TEST(config7);
	 ADD_TEST(config8);
	 ADD_TEST(config9);
	 ADD_TEST(config10);
	 ADD_TEST(config11);
	 ADD_TEST(config12);
	 ADD_TEST(config13);
	 ADD_TEST(config14);
	 ADD_TEST(config15);
	 ADD_TEST(config16);
	 ADD_TEST(config17);
END_SUITE
