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

void cl_git_append2file(const char *filename, const char *new_content)
{
	int fd = p_open(filename, O_WRONLY | O_APPEND | O_CREAT);
	cl_assert(fd != 0);
	if (!new_content)
		new_content = "\n";
	cl_must_pass(p_write(fd, new_content, strlen(new_content)));
	cl_must_pass(p_close(fd));
	cl_must_pass(p_chmod(filename, 0644));
}

static const char *_cl_sandbox = NULL;
static git_repository *_cl_repo = NULL;

git_repository *cl_git_sandbox_init(const char *sandbox)
{
	/* Copy the whole sandbox folder from our fixtures to our test sandbox
	 * area.  After this it can be accessed with `./sandbox`
	 */
	cl_fixture_sandbox(sandbox);
	_cl_sandbox = sandbox;

	p_chdir(sandbox);

	/* Rename `sandbox/.gitted` to `sandbox/.git` which must be done since
	 * we cannot store a folder named `.git` inside the fixtures folder of
	 * our libgit2 repo.
	 */
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

	p_chdir("..");

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

void locate_loose_object(const char *repository_folder, git_object *object, char **out, char **out_folder)
{
	static const char *objects_folder = "objects/";

	char *ptr, *full_path, *top_folder;
	int path_length, objects_length;

	assert(repository_folder && object);

	objects_length = strlen(objects_folder);
	path_length = strlen(repository_folder);
	ptr = full_path = git__malloc(path_length + objects_length + GIT_OID_HEXSZ + 3);

	strcpy(ptr, repository_folder);
	strcpy(ptr + path_length, objects_folder);

	ptr = top_folder = ptr + path_length + objects_length;
	*ptr++ = '/';
	git_oid_pathfmt(ptr, git_object_id(object));
	ptr += GIT_OID_HEXSZ + 1;
	*ptr = 0;

	*out = full_path;

	if (out_folder)
		*out_folder = top_folder;
}

int remove_loose_object(const char *repository_folder, git_object *object)
{
	char *full_path, *top_folder;

	locate_loose_object(repository_folder, object, &full_path, &top_folder);

	if (p_unlink(full_path) < 0) {
		fprintf(stderr, "can't delete object file \"%s\"\n", full_path);
		return -1;
	}

	*top_folder = 0;

	if ((p_rmdir(full_path) < 0) && (errno != ENOTEMPTY)) {
		fprintf(stderr, "can't remove object directory \"%s\"\n", full_path);
		return -1;
	}

	git__free(full_path);

	return GIT_SUCCESS;
}
