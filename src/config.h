#ifndef INCLUDE_config_h__
#define INCLUDE_config_h__

#include "git2.h"
#include "git2/config.h"
#include "vector.h"
#include "repository.h"

#define GIT_CONFIG_FILENAME ".gitconfig"
#define GIT_CONFIG_FILENAME_INREPO "config"

struct git_config {
	git_vector files;
	git_repository *repo;
};

#endif
