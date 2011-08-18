#ifndef INCLUDE_fetch_h__
#define INCLUDE_fetch_h__

int git_fetch_negotiate(git_remote *remote);
int git_fetch_download_pack(char **out, git_remote *remote);

#endif
