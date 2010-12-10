#ifndef INCLUDE_person_h__
#define INCLUDE_person_h__

#include "git2/common.h"
#include "repository.h"
#include <time.h>

/** Parsed representation of a person */
struct git_person {
	char *name; /**< Full name */
	char *email; /**< Email address */
	time_t time; /**< Time when this person committed the change */
	int timezone_offset; /**< Time zone offset in minutes. Can be either positive or negative. */
};

void git_person__free(git_person *person);
git_person *git_person__new(const char *name, const char *email, time_t time, int offset);
int git_person__parse(git_person *person, char **buffer_out, const char *buffer_end, const char *header);
int git_person__write(git_odb_source *src, const char *header, const git_person *person);

#endif
