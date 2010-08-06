#ifndef INCLUDE_tag_h__
#define INCLUDE_tag_h__

#include "git/tag.h"
#include "revobject.h"

struct git_tag {
	git_revpool_object object;

	git_revpool_object *target;
	git_otype type;
	char *tag_name;
	git_person *tagger;
	char *message;
};

void git_tag__free(git_tag *tag);
int git_tag__parse(git_tag *tag);

#endif
