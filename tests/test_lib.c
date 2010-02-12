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

#define GIT__NO_HIDE_MALLOC
#include "test_lib.h"

struct test_info {
	struct test_info *next;
	const char *test_name;
	const char *file_name;
	int line_no;
};

static int first_test = 1;
static struct test_info *current_test;

static void show_test_result(const char *status)
{
	fprintf(stderr, "* %-6s %5d: %s\n",
		status,
		current_test->line_no,
		current_test->test_name);
}

void test_die(const char *fmt, ...)
{
	va_list p;

	if (current_test)
		show_test_result("FAILED");

	va_start(p, fmt);
	vfprintf(stderr, fmt, p);
	va_end(p);
	fputc('\n', stderr);
	fflush(stderr);
	exit(128);
}

void test_begin(
	const char *test_name,
	const char *file_name,
	int line_no)
{
	struct test_info *i = malloc(sizeof(*i));
	if (!i)
		test_die("cannot malloc memory");

	i->test_name = test_name;
	i->file_name = file_name;
	i->line_no = line_no;
	current_test = i;

	if (first_test) {
		const char *name = strrchr(i->file_name, '/');
		if (name)
			name = name + 1;
		else
			name = i->file_name;
		fprintf(stderr, "*** %s ***\n", name);
		first_test = 0;
	}
}

void test_end(void)
{
	if (!current_test)
		test_die("BEGIN_TEST() not used before END_TEST");

	show_test_result("ok");
	free(current_test);
	current_test = NULL;
}
