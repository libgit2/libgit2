/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_thread_utils_h__
#define INCLUDE_git_thread_utils_h__

/*
 * How TLS works is compiler+platform dependant
 * Sources: http://en.wikipedia.org/wiki/Thread-Specific_Storage
 *			http://predef.sourceforge.net/precomp.html
 */

#ifdef GIT_THREADS
#	define GIT_HAS_TLS 1

/* No TLS in Cygwin */
#	if defined(__CHECKER__) || defined(__CYGWIN__)
#		undef GIT_HAS_TLS
#		define GIT_TLS

/* No TLS in Mach binaries for Mac OS X */
#	elif defined(__APPLE__) && defined(__MACH__)
#		undef GIT_TLS
#		define GIT_TLS

/* Normal TLS for GCC */
#	elif defined(__GNUC__) || \
		defined(__SUNPRO_C) || \
		defined(__SUNPRO_CC) || \
		defined(__xlc__) || \
		defined(__xlC__)
#		define GIT_TLS __thread

/* ICC may run on Windows or Linux */
#	elif defined(__INTEL_COMPILER)
#		if defined(_WIN32) || defined(_WIN32_CE)
#		define GIT_TLS __declspec(thread)
#		else
#		define GIT_TLS __thread
#		endif

/* Declspec for MSVC in Win32 */
#	elif defined(_WIN32) || \
		defined(_WIN32_CE) || \
		defined(__BORLANDC__)
#		define GIT_TLS __declspec(thread)

/* Other platform; no TLS */
#	else
#		undef GIT_HAS_TLS
#		define GIT_TLS /* nothing: tls vars are thread-global */
#	endif
#else /* Disable TLS if libgit2 is not threadsafe */
#	define GIT_TLS
#endif /* GIT_THREADS */

#endif /* INCLUDE_git_thread_utils_h__ */
