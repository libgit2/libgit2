/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifndef CLI_console_h__
#define CLI_console_h__

/**
 * Gets the current coordinates of the console, as pointed to by the given
 * file descriptor (to a stdout or stderr attached to a tty).  If the given
 * file descriptor is not a tty, then the columns and rows will not be set
 * and an error will be returned.
 *
 * @param cols Pointer to write the number of columns to
 * @param rows Pointer to write the number of rows to
 * @param fd The file descriptor for the console
 * @return 0 on success, -1 on failure
 */
int cli_console_coords(int *cols, int *rows, int fd);

#endif /* CLI_console_h__ */
