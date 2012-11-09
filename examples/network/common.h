#ifndef __COMMON_H__
#define __COMMON_H__

#include <git2.h>

typedef int (*git_cb)(git_repository *, int , char **);

int ls_remote(git_repository *repo, int argc, char **argv);
int parse_pkt_line(git_repository *repo, int argc, char **argv);
int show_remote(git_repository *repo, int argc, char **argv);
int fetch(git_repository *repo, int argc, char **argv);
int index_pack(git_repository *repo, int argc, char **argv);
int do_clone(git_repository *repo, int argc, char **argv);

#ifndef PRIuZ
/* Define the printf format specifer to use for size_t output */
#if defined(_MSC_VER) || defined(__MINGW32__)
#	define PRIuZ "Iu"
#else
#	define PRIuZ "zu"
#endif
#endif

#endif /* __COMMON_H__ */
