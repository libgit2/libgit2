#include "common.h"
#include "git2/filter.h"
#include "filter_helpers.h"
#include "clar_libgit2.h"

/*
 * Defining below a filter that applies on paths starting with
 * "hero".
 */
static int should_apply_to_path(
	git_filter *self, git_repository *repo, const char *path,
	git_filter_mode_t mode)
{
	GIT_UNUSED(self);
	GIT_UNUSED(repo);
	GIT_UNUSED(mode);

	if (!strncmp("hero", path, 4))
		return 1;
		
	return 0;
}

static int apply_to_odb(
	git_filter *self, git_repository *repo,
	const char *path, const char *source, size_t source_size,
	char **dst, size_t *dest_size)
{
	GIT_UNUSED(self);
	GIT_UNUSED(repo);
	GIT_UNUSED(path);

	memcpy(&dst, source, source_size);
	*dst[0] = 'a';
	*dest_size = source_size;

	return 0;
}

static int apply_to_worktree(
	git_filter *self, git_repository *repo,
	const char *path, const char *source, size_t source_size,
	char **dst, size_t *dest_size)
{
	GIT_UNUSED(self);
	GIT_UNUSED(repo);
	GIT_UNUSED(path);

	memcpy(&dst, source, source_size);
	*dst[0] = 'z';
	*dest_size = source_size;

	return 0;
}

static void do_free(git_filter *self)
{
	git_filter_free(self);
}

int create_custom_filter(git_filter **out, char *name)
{
	return git_filter_create_filter(out, should_apply_to_path,
		apply_to_odb, apply_to_worktree, do_free, name);
}
