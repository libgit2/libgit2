#ifndef INCLUDE_posix__w32_h__
#define INCLUDE_posix__w32_h__

#include "common.h"

#define p_lstat(p,b) lstat(p,b)
#define p_readlink(a, b, c) readlink(a, b, c)
#define p_link(o,n) link(o, n)
#define p_unlink(p) unlink(p)
#define p_mkdir(p,m) mkdir(p, m)
#define p_fsync(fd) fsync(fd)
#define p_realpath(p, po) realpath(p, po)

#endif
