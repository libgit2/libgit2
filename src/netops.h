/*
 * netops.h - convencience functions for networking
 */
#ifndef INCLUDE_netops_h__
#define INCLUDE_netops_h__

int gitno_connect(const char *host, const char *port);
int gitno_send(int s, const char *msg, int len, int flags);

#endif
