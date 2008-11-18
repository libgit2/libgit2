/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDE_git_os_abstraction_h__
#define INCLUDE_git_os_abstraction_h__

/** Force 64 bit off_t size on POSIX. */
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file git/os/unix.h
 * @brief Portable operating system abstractions
 * @defgroup git_os_abstraction Portable operating system abstractions
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Descriptor to an open file in the filesystem. */
typedef int git_file;

/**
 * Open a file by path name.
 *
 * Valid flags are:
 * - O_CREAT: Create the file if it does not yet exist.
 * - O_RDONLY: Open the file for reading.
 * - O_WRONLY: Open the file for writing.
 * - O_RDWR: Open the file for both reading and writing.
 *
 * @param out descriptor storage to populate on success.
 * @param path path name of the file to open.
 * @param flags bitmask of access requested to the file.
 * @return
 * - On success, GIT_SUCCESS.
 * - On error, <0.
 */
GIT_EXTERN(int) git_fopen(git_file *out, const char *path, int flags);

/**
 * Read from an open file descriptor at the current position.
 *
 * Exactly the requested number of bytes is read.  If the stream
 * ends early, an error is indicated, and the exact number of bytes
 * transferred is unspecified.
 *
 * @param fd open descriptor.
 * @param buf buffer to store the read data into.
 * @param cnt number of bytes to transfer.
 * @return
 * - On success, GIT_SUCCESS.
 * - On error, <0.
 */
GIT_EXTERN(int) git_fread(git_file fd, void *buf, size_t cnt);

/**
 * Write to an open file descriptor at the current position.
 *
 * Exactly the requested number of bytes is written.  If the stream
 * ends early, an error is indicated, and the exact number of bytes
 * transferred is unspecified.
 *
 * @param fd open descriptor.
 * @param buf buffer to write data from.
 * @param cnt number of bytes to transfer.
 * @return
 * - On success, GIT_SUCCESS.
 * - On error, <0.
 */
GIT_EXTERN(int) git_fwrite(git_file fd, void *buf, size_t cnt);

/**
 * Get the current size of an open file.
 * @param fd open descriptor.
 * @return
 * - On success, >= 0, indicating the file size in bytes.
 * - On error, <0.
 */
GIT_EXTERN(off_t) git_fsize(git_file fd);

/**
 * Close an open file descriptor.
 * @param fd descriptor to close.
 * @return
 * - On success, GIT_SUCCESS.
 * - On error, <0.
 */
#define git_fclose(fd) close(fd)

/** @} */
GIT_END_DECL
#endif
