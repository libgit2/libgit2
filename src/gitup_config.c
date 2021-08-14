#include "git2/gitup_config.h"
#include "git2/repository.h"

int gitup_config_find_local(git_repository *repo, git_buf *path)
{
	return git_repository_item_path(path, repo, GIT_REPOSITORY_ITEM_CONFIG);
}
