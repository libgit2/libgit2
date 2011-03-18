#ifndef INCLUDE_tag_h__
#define INCLUDE_tag_h__

#include "git2/tag.h"
#include "repository.h"
#include "odb.h"

struct git_tag {
	git_object object;

	git_oid target;
	git_otype type;

	char *tag_name;
	git_signature *tagger;
	char *message;
};

void git_tag__free(git_tag *tag);
int git_tag__parse(git_tag *tag, git_odb_object *obj);

#endif
