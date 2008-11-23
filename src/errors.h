#ifndef INCLUDE_errors_h__
#define INCLUDE_errors_h__
#include "git/errors.h"
#include <stdlib.h>

/* convenience functions */
static inline int git_int_error(int code)
{
	git_errno = code;
	return code;
}

static inline void *git_ptr_error(int code)
{
	git_errno = code;
	return NULL;
}
#endif
