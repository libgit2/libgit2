#ifndef INCLUDE_tag_h__
#define INCLUDE_tag_h__

#include "git2/tag.h"
#include "repository.h"

struct git_tag {
	git_object object;

	git_object *target;
	git_otype type;
	char *tag_name;
	git_person *tagger;
	char *message;
};

void git_tag__free(git_tag *tag);
int git_tag__parse(git_tag *tag);
int git_tag__writeback(git_tag *tag, git_odb_source *src);

#endif
