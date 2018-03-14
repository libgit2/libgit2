/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef INCLUDE_alloc_h__
#define INCLUDE_alloc_h__

#include "common.h"

#if defined(GIT_MSVC_CRTDBG)

/* Enable MSVC CRTDBG memory leak reporting.
 *
 * We DO NOT use the "_CRTDBG_MAP_ALLOC" macro described in the MSVC
 * documentation because all allocs/frees in libgit2 already go through
 * the "git__" routines defined in this file.  Simply using the normal
 * reporting mechanism causes all leaks to be attributed to a routine
 * here in util.h (ie, the actual call to calloc()) rather than the
 * caller of git__calloc().
 *
 * Therefore, we declare a set of "git__crtdbg__" routines to replace
 * the corresponding "git__" routines and re-define the "git__" symbols
 * as macros.  This allows us to get and report the file:line info of
 * the real caller.
 *
 * We DO NOT replace the "git__free" routine because it needs to remain
 * a function pointer because it is used as a function argument when
 * setting up various structure "destructors".
 *
 * We also DO NOT use the "_CRTDBG_MAP_ALLOC" macro because it causes
 * "free" to be remapped to "_free_dbg" and this causes problems for
 * structures which define a field named "free".
 *
 * Finally, CRTDBG must be explicitly enabled and configured at program
 * startup.  See tests/main.c for an example.
 */

#include "win32/w32_crtdbg_stacktrace.h"

#define git__malloc(len)                      git__crtdbg__malloc(len, __FILE__, __LINE__)
#define git__calloc(nelem, elsize)            git__crtdbg__calloc(nelem, elsize, __FILE__, __LINE__)
#define git__strdup(str)                      git__crtdbg__strdup(str, __FILE__, __LINE__)
#define git__strndup(str, n)                  git__crtdbg__strndup(str, n, __FILE__, __LINE__)
#define git__substrdup(str, n)                git__crtdbg__substrdup(str, n, __FILE__, __LINE__)
#define git__realloc(ptr, size)               git__crtdbg__realloc(ptr, size, __FILE__, __LINE__)
#define git__reallocarray(ptr, nelem, elsize) git__crtdbg__reallocarray(ptr, nelem, elsize, __FILE__, __LINE__)
#define git__mallocarray(nelem, elsize)       git__crtdbg__mallocarray(nelem, elsize, __FILE__, __LINE__)
#define git__free                             git__crtdbg__free

#else

#include "stdalloc.h"

#define git__malloc(len)                      git__stdalloc__malloc(len)
#define git__calloc(nelem, elsize)            git__stdalloc__calloc(nelem, elsize)
#define git__strdup(str)                      git__stdalloc__strdup(str)
#define git__strndup(str, n)                  git__stdalloc__strndup(str, n)
#define git__substrdup(str, n)                git__stdalloc__substrdup(str, n)
#define git__realloc(ptr, size)               git__stdalloc__realloc(ptr, size)
#define git__reallocarray(ptr, nelem, elsize) git__stdalloc__reallocarray(ptr, nelem, elsize)
#define git__mallocarray(nelem, elsize)       git__stdalloc__mallocarray(nelem, elsize)
#define git__free                             git__stdalloc__free

#endif /* !MSVC_CTRDBG */

#endif
