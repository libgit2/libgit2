#ifndef INCLUDE_blob_h__
#define INCLUDE_blob_h__

#include "git2/blob.h"
#include "repository.h"
#include "fileops.h"

struct git_blob {
	git_object object;
	gitfo_buf content;
};

void git_blob__free(git_blob *blob);
int git_blob__parse(git_blob *blob);
int git_blob__writeback(git_blob *blob, git_odb_source *src);

#endif
