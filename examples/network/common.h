#ifndef __COMMON_H__
#define __COMMON_H__

#include <git2.h>

typedef int (*git_cb)(git_repository *, int , char **);

int ls_remote(git_repository *repo, int argc, char **argv);
int parse_pkt_line(git_repository *repo, int argc, char **argv);
int show_remote(git_repository *repo, int argc, char **argv);
int fetch(git_repository *repo, int argc, char **argv);
int index_pack(git_repository *repo, int argc, char **argv);

#endif /* __COMMON_H__ */
