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

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
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
 * @param path path name of the file to open.
 * @param flags bitmask of access requested to the file.
 * @return the opened file descriptor; <0 if the open failed.
 */
static inline git_file git_fopen(const char *path, int flags)
{
	return open(path, flags);
}

/**
 * Close an open file descriptor.
 * @param fd descriptor to close.
 * @return 0 on success; <0 if the descriptor close failed.
 */
static inline int git_fclose(git_file fd)
{
	return close(fd);
}

/**
 * Read from an open file descriptor at the current position.
 *
 * Less than the number of requested bytes may be read.  The
 * read is automatically restarted if it fails due to a signal
 * being delivered to the calling thread.
 *
 * @param fd open descriptor.
 * @param buf buffer to store the read data into.
 * @param cnt number of bytes to transfer.
 * @return
 *  - On success, actual number of bytes read.
 *  - On EOF, 0.
 *  - On failure, <0.
 */
GIT_EXTERN(ssize_t) git_fread(git_file fd, void *buf, size_t cnt);

/**
 * Write to an open file descriptor at the current position.
 *
 * Less than the number of requested bytes may be written.  The
 * write is automatically restarted if it fails due to a signal
 * being delivered to the calling thread.
 *
 * @param fd open descriptor.
 * @param buf buffer to write data from.
 * @param cnt number of bytes to transfer.
 * @return
 *  - On success, actual number of bytes written.
 *  - On EOF, 0.
 *  - On failure, <0.
 */
GIT_EXTERN(ssize_t) git_fwrite(git_file fd, void *buf, size_t cnt);

/** @} */
GIT_END_DECL
#endif
