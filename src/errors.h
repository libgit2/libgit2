#ifndef INCLUDE_errors_h__
#define INCLUDE_errors_h__

#include "git/errors.h"

/* convenience functions */
GIT_INLINE(int) git_int_error(int code)
{
	git_errno = code;
	return code;
}

GIT_INLINE(int) git_os_error(void)
{
	return git_int_error(GIT_EOSERR);
}

GIT_INLINE(void) *git_ptr_error(int code)
{
	git_errno = code;
	return NULL;
}

#endif
