#include "clar_libgit2.h"
#include "posix.h"
#include "path.h"

void cl_git_report_failure(
	int error, const char *file, int line, const char *fncall)
{
	char msg[4096];
	const git_error *last = giterr_last();
	p_snprintf(msg, 4096, "error %d - %s",
		error, last ? last->message : "<no message>");
	clar__assert(0, file, line, fncall, msg, 1);
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
	wchar_t name_utf16[GIT_WIN_PATH];
	DWORD alloc_len;
	wchar_t *value_utf16;
	char *value_utf8;

	git__utf8_to_16(name_utf16, GIT_WIN_PATH, name);
	alloc_len = GetEnvironmentVariableW(name_utf16, NULL, 0);
	if (alloc_len <= 0)
		return NULL;

	alloc_len = GIT_WIN_PATH;
	cl_assert(value_utf16 = git__calloc(alloc_len, sizeof(wchar_t)));

	GetEnvironmentVariableW(name_utf16, value_utf16, alloc_len);

	cl_assert(value_utf8 = git__malloc(alloc_len));
	git__utf16_to_8(value_utf8, value_utf16);

	git__free(value_utf16);

	return value_utf8;
}

int cl_setenv(const char *name, const char *value)
{
	wchar_t name_utf16[GIT_WIN_PATH];
	wchar_t value_utf16[GIT_WIN_PATH];

	git__utf8_to_16(name_utf16, GIT_WIN_PATH, name);

	if (value) {
		git__utf8_to_16(value_utf16, GIT_WIN_PATH, value);
		cl_assert(SetEnvironmentVariableW(name_utf16, value_utf16));
	} else {
		/* Windows XP returns 0 (failed) when passing NULL for lpValue when
		 * lpName does not exist in the environment block. This behavior
		 * seems to have changed in later versions. Don't check return value
		 * of SetEnvironmentVariable when passing NULL for lpValue.
		 */
		SetEnvironmentVariableW(name_utf16, NULL);
	}

	return 0;
}

/* This function performs retries on calls to MoveFile in order
 * to provide enhanced reliability in the face of antivirus
 * agents that may be scanning the source (or in the case that
 * the source is a directory, a child of the source). */
int cl_rename(const char *source, const char *dest)
{
	wchar_t source_utf16[GIT_WIN_PATH];
	wchar_t dest_utf16[GIT_WIN_PATH];
	unsigned retries = 1;

	git__utf8_to_16(source_utf16, GIT_WIN_PATH, source);
	git__utf8_to_16(dest_utf16, GIT_WIN_PATH, dest);

	while (!MoveFileW(source_utf16, dest_utf16)) {
		/* Only retry if the error is ERROR_ACCESS_DENIED;
		 * this may indicate that an antivirus agent is
		 * preventing the rename from source to target */
		if (retries > 5 ||
			ERROR_ACCESS_DENIED != GetLastError())
			return -1;

		/* With 5 retries and a coefficient of 10ms, the maximum
		 * delay here is 550 ms */
		Sleep(10 * retries * retries);
		retries++;
	}

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

int cl_rename(const char *source, const char *dest)
{
	return p_rename(source, dest);
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
		cl_git_pass(cl_rename(".gitted", ".git"));

	/* If we have `gitattributes`, rename to `.gitattributes`.  This may
	 * be necessary if we don't want the attributes to be applied in the
	 * libgit2 repo, but just during testing.
	 */
	if (p_access("gitattributes", F_OK) == 0)
		cl_git_pass(cl_rename("gitattributes", ".gitattributes"));

	/* As with `gitattributes`, we may need `gitignore` just for testing. */
	if (p_access("gitignore", F_OK) == 0)
		cl_git_pass(cl_rename("gitignore", ".gitignore"));

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

const char* cl_git_fixture_url(const char *fixturename)
{
	return cl_git_path_url(cl_fixture(fixturename));
}

const char* cl_git_path_url(const char *path)
{
	static char url[4096];

	const char *in_buf;
	git_buf path_buf = GIT_BUF_INIT;
	git_buf url_buf = GIT_BUF_INIT;

	cl_git_pass(git_path_prettify_dir(&path_buf, path, NULL));
	cl_git_pass(git_buf_puts(&url_buf, "file://"));

#ifdef GIT_WIN32
	/*
	 * A FILE uri matches the following format: file://[host]/path
	 * where "host" can be empty and "path" is an absolute path to the resource.
	 *
	 * In this test, no hostname is used, but we have to ensure the leading triple slashes:
	 *
	 * *nix: file:///usr/home/...
	 * Windows: file:///C:/Users/...
	 */
	cl_git_pass(git_buf_putc(&url_buf, '/'));
#endif

	in_buf = git_buf_cstr(&path_buf);

	/*
	 * A very hacky Url encoding that only takes care of escaping the spaces
	 */
	while (*in_buf) {
		if (*in_buf == ' ')
			cl_git_pass(git_buf_puts(&url_buf, "%20"));
		else
			cl_git_pass(git_buf_putc(&url_buf, *in_buf));

		in_buf++;
	}

	strncpy(url, git_buf_cstr(&url_buf), 4096);
	git_buf_free(&url_buf);
	git_buf_free(&path_buf);
	return url;
}

typedef struct {
	const char *filename;
	size_t filename_len;
} remove_data;

static int remove_placeholders_recurs(void *_data, git_buf *path)
{
	remove_data *data = (remove_data *)_data;
	size_t pathlen;

	if (git_path_isdir(path->ptr) == true)
		return git_path_direach(path, remove_placeholders_recurs, data);

	pathlen = path->size;

	if (pathlen < data->filename_len)
		return 0;

	/* if path ends in '/'+filename (or equals filename) */
	if (!strcmp(data->filename, path->ptr + pathlen - data->filename_len) &&
		(pathlen == data->filename_len ||
		 path->ptr[pathlen - data->filename_len - 1] == '/'))
		return p_unlink(path->ptr);

	return 0;
}

int cl_git_remove_placeholders(const char *directory_path, const char *filename)
{
	int error;
	remove_data data;
	git_buf buffer = GIT_BUF_INIT;

	if (git_path_isdir(directory_path) == false)
		return -1;

	if (git_buf_sets(&buffer, directory_path) < 0)
		return -1;

	data.filename = filename;
	data.filename_len = strlen(filename);

	error = remove_placeholders_recurs(&data, &buffer);

	git_buf_free(&buffer);

	return error;
}
