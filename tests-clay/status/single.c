#include "clay_libgit2.h"
#include "posix.h"

static void
cleanup__remove_file(void *_file)
{
	cl_must_pass(p_unlink((char *)_file));
}

static void
file_create(const char *filename, const char *content)
{
	int fd = p_creat(filename, 0666);
	cl_assert(fd >= 0);
	cl_must_pass(p_write(fd, content, strlen(content)));
	cl_must_pass(p_close(fd));
}

/* test retrieving OID from a file apart from the ODB */
void test_status_single__hash_single_file(void)
{
	static const char file_name[] = "new_file";
	static const char file_contents[] = "new_file\n";
	static const char file_hash[] = "d4fa8600b4f37d7516bef4816ae2c64dbf029e3a";

	git_oid expected_id, actual_id;

	/* initialization */
	git_oid_fromstr(&expected_id, file_hash);
	file_create(file_name, file_contents);
	cl_set_cleanup(&cleanup__remove_file, (void *)file_name);

	cl_git_pass(git_odb_hashfile(&actual_id, file_name, GIT_OBJ_BLOB));
	cl_assert(git_oid_cmp(&expected_id, &actual_id) == 0);
}



