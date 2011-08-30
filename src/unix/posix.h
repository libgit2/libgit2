#ifndef INCLUDE_posix__w32_h__
#define INCLUDE_posix__w32_h__

#include <fnmatch.h>

#define p_lstat(p,b) lstat(p,b)
#define p_readlink(a, b, c) readlink(a, b, c)
#define p_link(o,n) link(o, n)
#define p_unlink(p) unlink(p)
#define p_mkdir(p,m) mkdir(p, m)
#define p_fsync(fd) fsync(fd)
#define p_realpath(p, po) realpath(p, po)
#define p_fnmatch(p, s, f) fnmatch(p, s, f)
#define p_vsnprintf(b, c, f, a) vsnprintf(b, c, f, a)
#define p_snprintf(b, c, f, ...) snprintf(b, c, f, __VA_ARGS__)
#define p_mkstemp(p) mkstemp(p)

#endif
