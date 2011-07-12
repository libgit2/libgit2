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
#include "buffer.h"

const char *test_string = "Have you seen that? Have you seeeen that??";

BEGIN_TEST(buf0, "check that resizing works properly")
	git_buf buf = GIT_BUF_INIT;
	git_buf_puts(&buf, test_string);

	must_be_true(git_buf_oom(&buf) == 0);
	must_be_true(strcmp(git_buf_cstr(&buf), test_string) == 0);

	git_buf_puts(&buf, test_string);
	must_be_true(strlen(git_buf_cstr(&buf)) == strlen(test_string) * 2);
	git_buf_free(&buf);
END_TEST

BEGIN_TEST(buf1, "check that printf works properly")
	git_buf buf = GIT_BUF_INIT;

	git_buf_printf(&buf, "%s %s %d ", "shoop", "da", 23);
	must_be_true(git_buf_oom(&buf) == 0);
	must_be_true(strcmp(git_buf_cstr(&buf), "shoop da 23 ") == 0);

	git_buf_printf(&buf, "%s %d", "woop", 42);
	must_be_true(git_buf_oom(&buf) == 0);
	must_be_true(strcmp(git_buf_cstr(&buf), "shoop da 23 woop 42") == 0);
	git_buf_free(&buf);
END_TEST

BEGIN_SUITE(buffers)
	 ADD_TEST(buf0)
	 ADD_TEST(buf1)
END_SUITE
