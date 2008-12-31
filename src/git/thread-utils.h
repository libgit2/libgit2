#ifndef INCLUDE_git_thread_utils_h__
#define INCLUDE_git_thread_utils_h__

/*
 * How TLS works is compiler+platform dependant
 * Sources: http://en.wikipedia.org/wiki/Thread-Specific_Storage
 *          http://predef.sourceforge.net/precomp.html
 */

#define GIT_HAS_TLS 1
#define GIT_HAS_PTHREAD 1

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
#  undef GIT_HAS_PTHREAD
# else
#  define GIT_TLS __thread
# endif

#elif defined(_WIN32) || \
      defined(_WIN32_CE) || \
      defined(__BORLANDC__)
# define GIT_TLS __declspec(thread)
# undef GIT_HAS_PTHREAD

#else
# undef GIT_HAS_TLS
# undef GIT_HAS_PTHREAD
# define GIT_TLS /* nothing: tls vars are thread-global */
#endif

/* sparse and cygwin don't grok thread-local variables */
#if defined(__CHECKER__) || defined(__CYGWIN__)
# undef GIT_HAS_TLS
# undef GIT_TLS
# define GIT_TLS
#endif

#ifdef GIT_HAS_PTHREAD
# define GIT_THREADS 1
#else
# undef GIT_THREADS
#endif

#endif /* INCLUDE_git_thread_utils_h__ */
