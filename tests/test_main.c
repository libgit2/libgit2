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
#include <string.h>

#undef BEGIN_TEST
#define BEGIN_TEST(name) extern void testfunc__##name(void);
#include TEST_TOC

struct test_def {
	const char *name;
	void (*fun)(void);
};
struct test_def all_tests[] = {
#   undef BEGIN_TEST
#   define BEGIN_TEST(name) {#name, testfunc__##name},
#   include TEST_TOC
	{NULL, NULL}
};

int main(int argc, char **argv)
{
	struct test_def *t;

	if (argc == 1) {
		for (t = all_tests; t->name; t++)
			t->fun();
		return 0;
	} else if (argc == 2) {
		for (t = all_tests; t->name; t++) {
			if (!strcmp(t->name, argv[1])) {
				t->fun();
				return 0;
			}
		}
		fprintf(stderr, "error: No test '%s' in %s\n", argv[1], argv[0]);
		return 1;
	} else {
		fprintf(stderr, "usage: %s [test_name]\n", argv[0]);
		return 1;
	}
}
