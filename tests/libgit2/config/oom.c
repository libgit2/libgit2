#include "clar_libgit2.h"

/*
 * An allocator that fails the Nth allocation, so that every allocation of an
 * operation can be made to fail in turn.
 */

static git_allocator std_alloc;
static git_allocator failing_alloc;
static size_t alloc_count;
static size_t fail_at;

static void *failing_malloc(size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.gmalloc(n, file, line);
}

static void *failing_realloc(void *p, size_t n, const char *file, int line)
{
	return (alloc_count++ == fail_at) ? NULL : std_alloc.grealloc(p, n, file, line);
}

void test_config_oom__initialize(void)
{
	git_stdalloc_init_allocator(&std_alloc);
	git_stdalloc_init_allocator(&failing_alloc);

	failing_alloc.gmalloc = failing_malloc;
	failing_alloc.grealloc = failing_realloc;

	cl_git_mkfile("config",
		"[core]\n"
		"\tfoo = bar\n"
		"\tmulti = one\n"
		"\tmulti = two\n"
		"[remote \"origin\"]\n"
		"\turl = https://example.com/repo.git\n");
}

void test_config_oom__cleanup(void)
{
	cl_git_pass(git_allocator_setup(NULL));
	cl_must_pass(p_unlink("config"));
}

void test_config_oom__open_and_snapshot_fail_or_succeed(void)
{
	git_config *cfg, *snapshot;
	git_buf foo, url;
	int error;

	/*
	 * Fail each allocation in turn until opening, snapshotting and
	 * reading the config needs no more of them: every step either
	 * reports the failure or gives the values in the file.
	 */
	for (fail_at = 0; ; fail_at++) {
		alloc_count = 0;
		cfg = snapshot = NULL;
		memset(&foo, 0, sizeof(foo));
		memset(&url, 0, sizeof(url));

		cl_git_pass(git_allocator_setup(&failing_alloc));
		if ((error = git_config_open_ondisk(&cfg, "config")) == 0 &&
		    (error = git_config_snapshot(&snapshot, cfg)) == 0 &&
		    (error = git_config_get_string_buf(&foo, snapshot, "core.foo")) == 0)
			error = git_config_get_string_buf(&url, snapshot, "remote.origin.url");
		cl_git_pass(git_allocator_setup(NULL));

		if (error == 0) {
			cl_assert_equal_s("bar", foo.ptr);
			cl_assert_equal_s("https://example.com/repo.git", url.ptr);
		}

		git_buf_dispose(&foo);
		git_buf_dispose(&url);
		git_config_free(snapshot);
		git_config_free(cfg);

		if (alloc_count <= fail_at) {
			/* no allocation failed, so everything must have succeeded */
			cl_git_pass(error);
			break;
		}
	}
}
