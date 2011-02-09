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

#include <string.h>
#include <git2.h>

#include "test_lib.h"
#include "test_helpers.h"

extern git_testsuite *libgit2_suite_core(void);
extern git_testsuite *libgit2_suite_rawobjects(void);
extern git_testsuite *libgit2_suite_objread(void);
extern git_testsuite *libgit2_suite_objwrite(void);
extern git_testsuite *libgit2_suite_commit(void);
extern git_testsuite *libgit2_suite_revwalk(void);
extern git_testsuite *libgit2_suite_index(void);
extern git_testsuite *libgit2_suite_hashtable(void);
extern git_testsuite *libgit2_suite_tag(void);
extern git_testsuite *libgit2_suite_tree(void);
extern git_testsuite *libgit2_suite_refs(void);
extern git_testsuite *libgit2_suite_sqlite(void);
extern git_testsuite *libgit2_suite_repository(void);

typedef git_testsuite *(*libgit2_suite)(void);

static libgit2_suite suite_methods[]= {
	libgit2_suite_core,
	libgit2_suite_rawobjects,
	libgit2_suite_objread,
	libgit2_suite_objwrite,
	libgit2_suite_commit,
	libgit2_suite_revwalk,
	libgit2_suite_index,
	libgit2_suite_hashtable,
	libgit2_suite_tag,
	libgit2_suite_tree,
	libgit2_suite_refs,
	libgit2_suite_sqlite,
	libgit2_suite_repository,
};

#define GIT_SUITE_COUNT (ARRAY_SIZE(suite_methods))


git_testsuite **libgit2_get_suites()
{
	git_testsuite **suites;
	unsigned int i;

	suites = git__malloc(GIT_SUITE_COUNT * sizeof(void *));
	if (suites == NULL)
		return NULL;

	for (i = 0; i < GIT_SUITE_COUNT; ++i)
		suites[i] = suite_methods[i]();

	return suites;
}

void libgit2_free_suites(git_testsuite **suites)
{
	unsigned int i;

	for (i = 0; i < GIT_SUITE_COUNT; ++i)
		git_testsuite_free(suites[i]);

	free(suites);
}

int main(int GIT_UNUSED(argc), char *GIT_UNUSED(argv[]))
{
	unsigned int i, failures;
	git_testsuite **suites;

	GIT_UNUSED_ARG(argc);
	GIT_UNUSED_ARG(argv);

	suites = libgit2_get_suites();
	failures = 0;

	for (i = 0; i < GIT_SUITE_COUNT; ++i)
		failures += git_testsuite_run(suites[i]);

	libgit2_free_suites(suites);

	return failures ? -1 : 0;
}

