#include "clar_libgit2.h"
#include "git2/odb_backend.h"

static git_repository *repo;
static git_odb *odb;

void test_odb_largefiles__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_odb(&odb, repo));
}

void test_odb_largefiles__cleanup(void)
{
	git_odb_free(odb);
	cl_git_sandbox_cleanup();
}

static void writefile(git_oid *oid)
{
	static git_odb_stream *stream;
	git_buf buf = GIT_BUF_INIT;
	size_t i;

	for (i = 0; i < 3041; i++)
		cl_git_pass(git_buf_puts(&buf, "Hello, world.\n"));

	cl_git_pass(git_odb_open_wstream(&stream, odb, 5368709122, GIT_OBJ_BLOB));
	for (i = 0; i < 126103; i++)
		cl_git_pass(git_odb_stream_write(stream, buf.ptr, buf.size));

	cl_git_pass(git_odb_stream_finalize_write(oid, stream));

	git_odb_stream_free(stream);
	git_buf_free(&buf);
}

void test_odb_largefiles__write_from_memory(void)
{
	git_oid expected, oid;
	git_buf buf = GIT_BUF_INIT;
	size_t i;

#ifndef GIT_ARCH_64
	cl_skip();
#endif

	if (!cl_is_env_set("GITTEST_INVASIVE_FS_SIZE") ||
		!cl_is_env_set("GITTEST_INVASIVE_MEMORY") ||
		!cl_is_env_set("GITTEST_SLOW"))
		cl_skip();

	for (i = 0; i < (3041*126103); i++)
		cl_git_pass(git_buf_puts(&buf, "Hello, world.\n"));

	git_oid_fromstr(&expected, "3fb56989cca483b21ba7cb0a6edb229d10e1c26c");
	cl_git_pass(git_odb_write(&oid, odb, buf.ptr, buf.size, GIT_OBJ_BLOB));

	cl_assert_equal_oid(&expected, &oid);
}

void test_odb_largefiles__streamwrite(void)
{
	git_oid expected, oid;

	if (!cl_is_env_set("GITTEST_INVASIVE_FS_SIZE") ||
		!cl_is_env_set("GITTEST_SLOW"))
		cl_skip();

	git_oid_fromstr(&expected, "3fb56989cca483b21ba7cb0a6edb229d10e1c26c");
	writefile(&oid);

	cl_assert_equal_oid(&expected, &oid);
}

void test_odb_largefiles__read_into_memory(void)
{
	git_oid oid;
	git_odb_object *obj;

#ifndef GIT_ARCH_64
	cl_skip();
#endif

	if (!cl_is_env_set("GITTEST_INVASIVE_FS_SIZE") ||
		!cl_is_env_set("GITTEST_INVASIVE_MEMORY") ||
		!cl_is_env_set("GITTEST_SLOW"))
		cl_skip();

	writefile(&oid);
	cl_git_pass(git_odb_read(&obj, odb, &oid));

	git_odb_object_free(obj);
}

void test_odb_largefiles__read_into_memory_rejected_on_32bit(void)
{
	git_oid oid;
	git_odb_object *obj = NULL;

#ifdef GIT_ARCH_64
	cl_skip();
#endif

	if (!cl_is_env_set("GITTEST_INVASIVE_FS_SIZE") ||
		!cl_is_env_set("GITTEST_INVASIVE_MEMORY") ||
		!cl_is_env_set("GITTEST_SLOW"))
		cl_skip();

	writefile(&oid);
	cl_git_fail(git_odb_read(&obj, odb, &oid));

	git_odb_object_free(obj);
}
