#include "common.h"
#include "thread-utils.h" /* for GIT_TLS */

static struct {
	int num;
	const char *str;
} error_codes[] = {
	{GIT_ERROR, "Unspecified error"},
	{GIT_ENOTOID, "Input was not a properly formatted Git object id."},
	{GIT_ENOTFOUND, "Object does not exist in the scope searched."},
	{GIT_ENOMEM, "Not enough space available."},
	{GIT_EOSERR, "Consult the OS error information."},
	{GIT_EOBJTYPE, "The specified object is of invalid type"},
	{GIT_EOBJCORRUPTED, "The specified object has its data corrupted"},
	{GIT_ENOTAREPO, "The specified repository is invalid"},
	{GIT_EINVALIDTYPE, "The object type is invalid or doesn't match"},
	{GIT_EMISSINGOBJDATA, "The object cannot be written that because it's missing internal data"},
	{GIT_EPACKCORRUPTED, "The packfile for the ODB is corrupted"},
	{GIT_EFLOCKFAIL, "Failed to adquire or release a file lock"},
	{GIT_EZLIB, "The Z library failed to inflate/deflate an object's data"},
	{GIT_EBUSY, "The queried object is currently busy"},
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
