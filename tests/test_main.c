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

#include "posix.h"

#include "test_lib.h"
#include "test_helpers.h"

DECLARE_SUITE(core);
DECLARE_SUITE(rawobjects);
DECLARE_SUITE(objread);
DECLARE_SUITE(objwrite);
DECLARE_SUITE(commit);
DECLARE_SUITE(revwalk);
DECLARE_SUITE(index);
DECLARE_SUITE(hashtable);
DECLARE_SUITE(tag);
DECLARE_SUITE(tree);
DECLARE_SUITE(refs);
DECLARE_SUITE(repository);
DECLARE_SUITE(threads);
DECLARE_SUITE(config);
DECLARE_SUITE(remotes);
DECLARE_SUITE(buffers);
DECLARE_SUITE(status);

static libgit2_suite suite_methods[]= {
	SUITE_NAME(core),
	SUITE_NAME(rawobjects),
	SUITE_NAME(objread),
	SUITE_NAME(objwrite),
	SUITE_NAME(commit),
	SUITE_NAME(revwalk),
	SUITE_NAME(index),
	SUITE_NAME(hashtable),
	SUITE_NAME(tag),
	SUITE_NAME(tree),
	SUITE_NAME(refs),
	SUITE_NAME(repository),
	SUITE_NAME(threads),
	SUITE_NAME(config),
	SUITE_NAME(remotes),
	SUITE_NAME(buffers),
	SUITE_NAME(status),
};

#define GIT_SUITE_COUNT (ARRAY_SIZE(suite_methods))

#ifdef GIT_WIN32
int __cdecl
#else
int
#endif
main(int GIT_UNUSED(argc), char *GIT_UNUSED(argv[]))
{
	unsigned int i, failures;

	GIT_UNUSED_ARG(argc);
	GIT_UNUSED_ARG(argv);

	p_umask(0);

	failures = 0;

	for (i = 0; i < GIT_SUITE_COUNT; ++i)
		failures += git_testsuite_run(suite_methods[i]());

	return failures ? -1 : 0;
}

