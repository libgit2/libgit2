#ifndef REFTABLE_SYSTEM_H
#define REFTABLE_SYSTEM_H

/*
 * This header defines the platform-specific bits required to compile the
 * reftable library. It should provide an environment that bridges over the
 * gaps between POSIX and your system, as well as the zlib interfaces. This
 * header is expected to be changed by the individual project.
 */

#include "map.h"
#include "posix.h"
#include "util.h"
#include <zlib.h>

/*
 * We only need to redefine on Windows as we expect stat(3p) et al to be
 * available on Unix platforms. Furthermore, we only need to redefine fstat(3p)
 * because we already redefine stat(3p) "win32-compat.h".
 */
#ifdef GIT_WIN32
#  define fstat(fd, st) p_fstat(fd, st)
#endif

#define fsync(fd) p_fsync(fd)

#define poll(fds, fds_len, timeout) p_poll(fds, fds_len, timeout)

#define REFTABLE_ALLOW_BANNED_ALLOCATORS

#define inline GIT_INLINE_KEYWORD

#endif
