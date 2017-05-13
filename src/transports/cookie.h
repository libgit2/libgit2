#ifndef HEADER_CURL_COOKIE_H
#define HEADER_CURL_COOKIE_H
/***************************************************************************
 * Copyright (C) 1998 - 2016, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include <stdbool.h>
#include <git2/types.h>

struct Cookie {
  struct Cookie *next; /* next in the chain */
  char *name;        /* <this> = value */
  char *value;       /* name = <this> */
  char *path;         /* path = <this> which is in Set-Cookie: */
  char *spath;        /* sanitized cookie path */
  char *domain;      /* domain = <this> */
  git_off_t expires;  /* expires = <this> */
  char *expirestr;   /* the plain text version */
  bool tailmatch;    /* weather we do tail-matchning of the domain name */

  /* RFC 2109 keywords. Version=1 means 2109-compliant cookie sending */
  char *version;     /* Version = <value> */
  char *maxage;      /* Max-Age = <value> */

  bool secure;       /* whether the 'secure' keyword was used */
  bool httponly;     /* true if the httponly directive is present */
};

struct CookieInfo {
  /* linked list of cookies we know of */
  struct Cookie *cookies;

  long numcookies; /* number of cookies in the "jar" */
};

/* This is the maximum line length we accept for a cookie line. RFC 2109
   section 6.3 says:

   "at least 4096 bytes per cookie (as measured by the size of the characters
   that comprise the cookie non-terminal in the syntax description of the
   Set-Cookie header)"

*/
#define MAX_COOKIE_LINE 5000
#define MAX_COOKIE_LINE_TXT "4999"

/* This is the maximum length of a cookie name we deal with: */
#define MAX_NAME 1024
#define MAX_NAME_TXT "1023"

/*
 * Add a cookie to the internal list of cookies. The domain and path arguments
 * are only used if the header boolean is TRUE.
 */

struct Cookie *cookie_add(struct CookieInfo *, char *lineptr);
struct Cookie *cookie_getlist(struct CookieInfo *, const char *,
                                   const char *, bool);
void cookie_freelist(struct Cookie *cookies);
void cookie_cleanup(struct CookieInfo *);
struct CookieInfo *cookie_loadfile(const char *cookie_file);

#endif /* HEADER_CURL_COOKIE_H */
