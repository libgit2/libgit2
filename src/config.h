#ifndef INCLUDE_config_h__
#define INCLUDE_config_h__

#include "git2.h"
#include "git2/config.h"
#include "vector.h"

struct git_config {
	git_vector backends;
};

void git__strtolower(char *str);
void git__strntolower(char *str, int len);

#endif
