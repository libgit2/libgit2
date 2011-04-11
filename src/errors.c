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
	{GIT_EINVALIDPATH, "The path is invalid"},
	{GIT_EBAREINDEX, "The index file is not backed up by an existing repository"},
	{GIT_EINVALIDREFNAME, "The name of the reference is not valid"},
	{GIT_EREFCORRUPTED, "The specified reference has its data corrupted"},
	{GIT_ETOONESTEDSYMREF, "The specified symbolic reference is too deeply nested"},
	{GIT_EPACKEDREFSCORRUPTED, "The pack-refs file is either corrupted of its format is not currently supported"},
	{GIT_EINVALIDPATH, "The path is invalid" },
	{GIT_EREVWALKOVER, "The revision walker is empty; there are no more commits left to iterate"},
	{GIT_EINVALIDREFSTATE, "The state of the reference is not valid"},
	{GIT_ENOTIMPLEMENTED, "This feature has not been implemented yet"},
	{GIT_EEXISTS, "A reference with this name already exists"},
	{GIT_EOVERFLOW, "The given integer literal is too large to be parsed"},
	{GIT_ENOTNUM, "The given literal is not a valid number"},
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
