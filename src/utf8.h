/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_utf8_h__
#define INCLUDE_utf8_h__

#include "common.h"

/*
 * Iterate through an UTF-8 string, yielding one codepoint at a time.
 *
 * @param out pointer where to store the current codepoint
 * @param str current position in the string
 * @param str_len size left in the string
 * @return length in bytes of the read codepoint; -1 if the codepoint was invalid
 */
extern int git_utf8_iterate(uint32_t *out, const char *str, size_t str_len);

/**
 * Iterate through an UTF-8 string and stops after finding any invalid UTF-8
 * codepoints.
 *
 * @param str string to scan
 * @param str_len size of the string
 * @return length in bytes of the string that contains valid data
 */
extern size_t git_utf8_valid_buf_length(const char *str, size_t str_len);

#endif
