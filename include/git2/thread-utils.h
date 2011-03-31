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
#ifndef INCLUDE_git_thread_utils_h__
#define INCLUDE_git_thread_utils_h__

/*
 * How TLS works is compiler+platform dependant
 * Sources: http://en.wikipedia.org/wiki/Thread-Specific_Storage
 *          http://predef.sourceforge.net/precomp.html
 */

#define GIT_HAS_TLS 1

#if defined(__APPLE__) && defined(__MACH__)
# undef GIT_TLS

#elif defined(__GNUC__) || \
      defined(__SUNPRO_C) || \
      defined(__SUNPRO_CC) || \
      defined(__xlc__) || \
      defined(__xlC__)
# define GIT_TLS __thread

#elif defined(__INTEL_COMPILER)
# if defined(_WIN32) || defined(_WIN32_CE)
#  define GIT_TLS __declspec(thread)
# else
#  define GIT_TLS __thread
# endif

#elif defined(_WIN32) || \
      defined(_WIN32_CE) || \
      defined(__BORLANDC__)
# define GIT_TLS __declspec(thread)

#else
# undef GIT_HAS_TLS
# define GIT_TLS /* nothing: tls vars are thread-global */
#endif

/* sparse and cygwin don't grok thread-local variables */
#if defined(__CHECKER__) || defined(__CYGWIN__)
# undef GIT_HAS_TLS
# undef GIT_TLS
# define GIT_TLS
#endif

#endif /* INCLUDE_git_thread_utils_h__ */
