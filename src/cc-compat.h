/*
 * cc-compat.h - C compiler compat macros for internal use
 */
#ifndef INCLUDE_compat_h__
#define INCLUDE_compat_h__

/*
 * See if our compiler is known to support flexible array members.
 */
#ifndef GIT_FLEX_ARRAY
# if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define GIT_FLEX_ARRAY /* empty */
# elif defined(__GNUC__)
#  if (__GNUC__ >= 3)
#   define GIT_FLEX_ARRAY /* empty */
#  else
#   define GIT_FLEX_ARRAY 0 /* older GNU extension */
#  endif
# endif

/* Default to safer but a bit wasteful traditional style */
# ifndef GIT_FLEX_ARRAY
#  define GIT_FLEX_ARRAY 1
# endif
#endif

#ifdef __GNUC__
# define GIT_TYPEOF(x) (__typeof__(x))
#else
# define GIT_TYPEOF(x)
#endif

/*
 * Does our compiler/platform support the C99 <inttypes.h> and
 * <stdint.h> header files. (C99 requires that <inttypes.h>
 * includes <stdint.h>).
 */
#if !defined(_MSC_VER)
# define GIT_HAVE_INTTYPES_H 1
#endif

/* Define the printf format specifer to use for size_t output */
#if !defined(_MSC_VER)
# define PRIuZ "zu"
#else
# define PRIuZ "Iu"
#endif

#endif /* INCLUDE_compat_h__ */
