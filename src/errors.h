#ifndef INCLUDE_errors_h__
#define INCLUDE_errors_h__

#include "git2/common.h"

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

#define git_error_quick_wrap(child, message) \
	git_error_quick_wrap(__FILE__, __LINE__, child, message)

/* Throw an out-of-memory error */
extern git_error * git_error_oom(void);

#endif /* INCLUDE_errors_h__ */
