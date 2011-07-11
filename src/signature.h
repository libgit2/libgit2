#ifndef INCLUDE_signature_h__
#define INCLUDE_signature_h__

#include "git2/common.h"
#include "git2/signature.h"
#include "repository.h"
#include <time.h>

int git_signature__parse(git_signature *sig, const char **buffer_out, const char *buffer_end, const char *header, char ender);
void git_signature__writebuf(git_buf *buf, const char *header, const git_signature *sig);

#endif
