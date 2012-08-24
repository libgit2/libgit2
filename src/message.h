/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_message_h__
#define INCLUDE_message_h__

#include "buffer.h"

int git_message_prettify(git_buf *message_out, const char *message, int strip_comments);

#endif /* INCLUDE_message_h__ */
