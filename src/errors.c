#include "common.h"
#include "thread-utils.h" /* for GIT_TLS */

#if defined(GIT_TLS)
/* compile-time constant initialization required */
GIT_TLS int git_errno = 0;

#elif defined(GIT_HAS_PTHREAD)

static pthread_key_t errno_key;

static void init_errno(void) __attribute__((constructor));
static void init_errno(void)
{
	pthread_key_create(&errno_key, free);
}

int *git__errno_storage(void)
{
	int *e = pthread_getspecific(errno_key);
	if (!e) {
#undef calloc
		e = calloc(1, sizeof(*e));
#define calloc(a,b) GIT__FORBID_MALLOC
		pthread_setspecific(errno_key, e);
	}
	return e;
}

#endif

static struct {
	int num;
	const char *str;
} error_codes[] = {
	{ GIT_ENOTOID, "Not a git oid" },
	{ GIT_ENOTFOUND, "Object does not exist in the scope searched" },
	{ GIT_ENOMEM, "Not enough space" },
};

const char *git_strerror(int num)
{
	size_t i;

	if (num == GIT_EOSERR)
		return strerror(errno);
	for (i = 0; i < ARRAY_SIZE(error_codes); i++)
		if (num == error_codes[i].num)
			return error_codes[i].str;

	return "Unknown error";
}
