#ifndef INCLUDE_errors_h__
#define INCLUDE_errors_h__

#include "git2/common.h"

/* Deprecated - please use the more advanced functions below. */
extern void git___throw(const char *, ...) GIT_FORMAT_PRINTF(1, 2);
#define git__throw(error, ...) \
	(git___throw(__VA_ARGS__), error)

/* Deprecated - please use the more advanced functions below. */
extern void git___rethrow(const char *, ...) GIT_FORMAT_PRINTF(1, 2);
#define git__rethrow(error, ...) \
	(git___rethrow(__VA_ARGS__), error)

/*
 * This implementation is loosely based on subversion's error
 * handling.
 */

git_error * git_error_create(const char *file, int line, int code,
			     git_error *child, const char *msg);

git_error * git_error_createf(const char *file, int line, int code,
			      git_error *child, const char *msg,
			      ...) GIT_FORMAT_PRINTF(5, 6);

git_error * git_error_quick_wrap(const char *file, int line,
				 git_error *child, const char *msg);

#define git_error_create(code, child, message) \
	git_error_create(__FILE__, __LINE__, code, child, message)

#define git_error_createf(code, child, format, ...) \
	git_error_createf(__FILE__, __LINE__, code, child, format, __VA_ARGS__)

/*
 * Wrap an error with a message. All git_error values are assigned with
 * child's fields.
 */
#define git_error_quick_wrap(child, message) \
	git_error_quick_wrap(__FILE__, __LINE__, child, message)

/*
 * Use this function to wrap functions like
 *
 *	git_error * foo(void)
 *	{
 *		return git_error_trace(bar());
 *	}
 *
 * Otherwise the call of foo() wouldn't be visible in the trace.
 *
 */
#define git_error_trace(error) \
	git_error_quick_wrap(error, "traced error");

/* Throw an out-of-memory error */
extern git_error * git_error_oom(void);

#endif /* INCLUDE_errors_h__ */
