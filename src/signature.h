#ifndef INCLUDE_signature_h__
#define INCLUDE_signature_h__

#include "git2/common.h"
#include "git2/signature.h"
#include "repository.h"
#include <time.h>

int git_signature__parse(git_signature *sig, char **buffer_out, const char *buffer_end, const char *header);
int git_signature__write(char **signature, const char *header, const git_signature *sig);

#endif
