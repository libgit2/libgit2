/*
 * cc-compat.h - C compiler compat macros for internal use
 */
#ifndef INCLUDE_compat_h__
#define INCLUDE_compat_h__

/*
 * See if our compiler is known to support flexible array members.
 */
#ifndef FLEX_ARRAY
# if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#  define FLEX_ARRAY /* empty */
# elif defined(__GNUC__)
#  if (__GNUC__ >= 3)
#   define FLEX_ARRAY /* empty */
#  else
#   define FLEX_ARRAY 0 /* older GNU extension */
#  endif
# endif

/* Default to safer but a bit wasteful traditional style */
# ifndef GIT_FLEX_ARRAY
#  define GIT_FLEX_ARRAY 1
# endif
#endif

#ifdef __GNUC__
# define TYPEOF(x) (__typeof__(x))
#else
# define TYPEOF(x)
#endif

#endif /* INCLUDE_compat_h__ */
