#include "clar_libgit2.h"
#include "posix.h"

void clar_on_init(void)
{
	git_threads_init();
}

void clar_on_shutdown(void)
{
	git_threads_shutdown();
}

void cl_git_mkfile(const char *filename, const char *content)
{
	int fd;

	fd = p_creat(filename, 0666);
	cl_assert(fd != 0);

	if (content) {
		cl_must_pass(p_write(fd, content, strlen(content)));
	} else {
		cl_must_pass(p_write(fd, filename, strlen(filename)));
		cl_must_pass(p_write(fd, "\n", 1));
	}

	cl_must_pass(p_close(fd));
}

void cl_git_write2file(
	const char *filename, const char *new_content, int flags, unsigned int mode)
{
	int fd = p_open(filename, flags, mode);
	cl_assert(fd >= 0);
	if (!new_content)
		new_content = "\n";
	cl_must_pass(p_write(fd, new_content, strlen(new_content)));
	cl_must_pass(p_close(fd));
}

void cl_git_append2file(const char *filename, const char *new_content)
{
	cl_git_write2file(filename, new_content, O_WRONLY | O_CREAT | O_APPEND, 0644);
}

void cl_git_rewritefile(const char *filename, const char *new_content)
{
	cl_git_write2file(filename, new_content, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

#ifdef GIT_WIN32

#include "win32/utf-conv.h"

char *cl_getenv(const char *name)
{
	wchar_t *name_utf16 = gitwin_to_utf16(name);
	DWORD value_len, alloc_len;
	wchar_t *value_utf16;
	char *value_utf8;

	cl_assert(name_utf16);
	alloc_len = GetEnvironmentVariableW(name_utf16, NULL, 0);
	if (alloc_len <= 0)
		return NULL;

	cl_assert(value_utf16 = git__calloc(alloc_len, sizeof(wchar_t)));

	value_len = GetEnvironmentVariableW(name_utf16, value_utf16, alloc_len);
	cl_assert_equal_i(value_len, alloc_len - 1);

	cl_assert(value_utf8 = gitwin_from_utf16(value_utf16));

	git__free(value_utf16);

	return value_utf8;
}

int cl_setenv(const char *name, const char *value)
{
	wchar_t *name_utf16 = gitwin_to_utf16(name);
	wchar_t *value_utf16 = value ? gitwin_to_utf16(value) : NULL;

	cl_assert(name_utf16);
	cl_assert(SetEnvironmentVariableW(name_utf16, value_utf16));

	git__free(name_utf16);
	git__free(value_utf16);

	return 0;

}
#else

#include <stdlib.h>
char *cl_getenv(const char *name)
{
   return getenv(name);
}

int cl_setenv(const char *name, const char *value)
{
	return (value == NULL) ? unsetenv(name) : setenv(name, value, 1);
}
#endif

static const char *_cl_sandbox = NULL;
static git_repository *_cl_repo = NULL;

git_repository *cl_git_sandbox_init(const char *sandbox)
{
	/* Copy the whole sandbox folder from our fixtures to our test sandbox
	 * area.  After this it can be accessed with `./sandbox`
	 */
	cl_fixture_sandbox(sandbox);
	_cl_sandbox = sandbox;

	cl_git_pass(p_chdir(sandbox));

	/* If this is not a bare repo, then rename `sandbox/.gitted` to
	 * `sandbox/.git` which must be done since we cannot store a folder
	 * named `.git` inside the fixtures folder of our libgit2 repo.
	 */
	if (p_access(".gitted", F_OK) == 0)
		cl_git_pass(p_rename(".gitted", ".git"));

	/* If we have `gitattributes`, rename to `.gitattributes`.  This may
	 * be necessary if we don't want the attributes to be applied in the
	 * libgit2 repo, but just during testing.
	 */
	if (p_access("gitattributes", F_OK) == 0)
		cl_git_pass(p_rename("gitattributes", ".gitattributes"));

	/* As with `gitattributes`, we may need `gitignore` just for testing. */
	if (p_access("gitignore", F_OK) == 0)
		cl_git_pass(p_rename("gitignore", ".gitignore"));

	cl_git_pass(p_chdir(".."));

	/* Now open the sandbox repository and make it available for tests */
	cl_git_pass(git_repository_open(&_cl_repo, sandbox));

	return _cl_repo;
}

void cl_git_sandbox_cleanup(void)
{
	if (_cl_repo) {
		git_repository_free(_cl_repo);
		_cl_repo = NULL;
	}
	if (_cl_sandbox) {
		cl_fixture_cleanup(_cl_sandbox);
		_cl_sandbox = NULL;
	}
}

bool cl_toggle_filemode(const char *filename)
{
	struct stat st1, st2;

	cl_must_pass(p_stat(filename, &st1));
	cl_must_pass(p_chmod(filename, st1.st_mode ^ 0100));
	cl_must_pass(p_stat(filename, &st2));

	return (st1.st_mode != st2.st_mode);
}

bool cl_is_chmod_supported(void)
{
	static int _is_supported = -1;

	if (_is_supported < 0) {
		cl_git_mkfile("filemode.t", "Test if filemode can be modified");
		_is_supported = cl_toggle_filemode("filemode.t");
		cl_must_pass(p_unlink("filemode.t"));
	}

	return _is_supported;
}

