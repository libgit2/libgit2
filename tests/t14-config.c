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

#define CONFIG_BASE TEST_RESOURCES "/config"

/*
 * This one is so we know the code isn't completely broken
 */
BEGIN_TEST(config0, "read a simple configuration")
	git_config *cfg;
	int i;

	must_pass(git_config_open(&cfg, CONFIG_BASE "/config0"));
	must_pass(git_config_get_int(cfg, "core.repositoryformatversion", &i));
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

	must_pass(git_config_open(&cfg, CONFIG_BASE "/config1"));

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

	must_pass(git_config_open(&cfg, CONFIG_BASE "/config2"));

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

	must_pass(git_config_open(&cfg, CONFIG_BASE "/config3"));

	must_pass(git_config_get_string(cfg, "section.subsection.var", &str));
	must_be_true(!strcmp(str, "hello"));

	/* Avoid a false positive */
	str = "nohello";
	must_pass(git_config_get_string(cfg, "section.subSectIon.var", &str));
	must_be_true(!strcmp(str, "hello"));

	git_config_free(cfg);
END_TEST

BEGIN_TEST(config4, "a variable name on its own is valid")
	git_config *cfg;
const char *str;
int i;

	must_pass(git_config_open(&cfg, CONFIG_BASE "/config4"));

	must_pass(git_config_get_string(cfg, "some.section.variable", &str));
	must_be_true(str == NULL);

	must_pass(git_config_get_bool(cfg, "some.section.variable", &i));
	must_be_true(i == 1);


	git_config_free(cfg);
END_TEST

BEGIN_SUITE(config)
	 ADD_TEST(config0);
	 ADD_TEST(config1);
	 ADD_TEST(config2);
	 ADD_TEST(config3);
	 ADD_TEST(config4);
END_SUITE
