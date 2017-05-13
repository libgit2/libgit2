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

#ifndef GIT_WINHTTP

#include "cookie.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define ISBLANK(x)  (int)((((unsigned char)x) == ' ') || \
			  (((unsigned char)x) == '\t'))
/*
 * get_line() makes sure to only return complete whole lines that fit in 'len'
 * bytes and end with a newline.
 */
static char *get_line(char *buf, int len, FILE *input)
{
	bool partial = false;
	while(1) {
		char *b = fgets(buf, len, input);
		if(b) {
			size_t rlen = strlen(b);
			if(rlen && (b[rlen-1] == '\n')) {
				if(partial) {
					partial = false;
					continue;
				}
				return b;
			}
			/* read a partial, discard the next piece that ends with newline */
			partial = true;
		}
		else
			break;
	}
	return NULL;
}

static void freecookie(struct Cookie *co)
{
	free(co->expirestr);
	free(co->domain);
	free(co->path);
	free(co->spath);
	free(co->name);
	free(co->value);
	free(co->maxage);
	free(co->version);
	free(co);
}

/*
 * cookie path sanitize
 */
static char *sanitize_cookie_path(const char *cookie_path)
{
	size_t len;
	char *new_path = strdup(cookie_path);
	if(!new_path)
		return NULL;

	/* some stupid site sends path attribute with '"'. */
	len = strlen(new_path);
	if(new_path[0] == '\"') {
		memmove((void *)new_path, (const void *)(new_path + 1), len);
		len--;
	}
	if(len && (new_path[len - 1] == '\"')) {
		new_path[len - 1] = 0x0;
		len--;
	}

	/* RFC6265 5.2.4 The Path Attribute */
	if(new_path[0] != '/') {
		/* Let cookie-path be the default-path. */
		free(new_path);
		new_path = strdup("/");
		return new_path;
	}

	/* convert /hoge/ to /hoge */
	if(len && new_path[len - 1] == '/') {
		new_path[len - 1] = 0x0;
	}

	return new_path;
}

/*
 * remove_expired() removes expired cookies.
 */
static void remove_expired(struct CookieInfo *cookies)
{
	struct Cookie *co, *nx, *pv;
	git_off_t now = (git_off_t)time(NULL);

	co = cookies->cookies;
	pv = NULL;
	while(co) {
		nx = co->next;
		if(co->expires && co->expires < now) {
			if(co == cookies->cookies) {
				cookies->cookies = co->next;
			}
			else {
				pv->next = co->next;
			}
			cookies->numcookies--;
			freecookie(co);
		}
		else {
			pv = co;
		}
		co = nx;
	}
}

/* Portable, consistent toupper (remember EBCDIC). Do not use toupper() because
   its behavior is altered by the current locale. */
char raw_toupper(char in)
{
	switch(in) {
		case 'a':
			return 'A';
		case 'b':
			return 'B';
		case 'c':
			return 'C';
		case 'd':
			return 'D';
		case 'e':
			return 'E';
		case 'f':
			return 'F';
		case 'g':
			return 'G';
		case 'h':
			return 'H';
		case 'i':
			return 'I';
		case 'j':
			return 'J';
		case 'k':
			return 'K';
		case 'l':
			return 'L';
		case 'm':
			return 'M';
		case 'n':
			return 'N';
		case 'o':
			return 'O';
		case 'p':
			return 'P';
		case 'q':
			return 'Q';
		case 'r':
			return 'R';
		case 's':
			return 'S';
		case 't':
			return 'T';
		case 'u':
			return 'U';
		case 'v':
			return 'V';
		case 'w':
			return 'W';
		case 'x':
			return 'X';
		case 'y':
			return 'Y';
		case 'z':
			return 'Z';
	}
	return in;
}

int strcasecompare(const char *first, const char *second)
{
	while(*first && *second) {
		if(raw_toupper(*first) != raw_toupper(*second))
			/* get out of the loop as soon as they don't
			 * match */
			break;
		first++;
		second++;
	}
	/* we do the comparison here (possibly again), just to make sure
	 * that if the
	 *      loop above is skipped because one of the strings reached
	 *      zero, we must not
	 *           return this as a successful match */
	return (raw_toupper(*first) == raw_toupper(*second));
}

/*
 * Return true if the given string is an IP(v4|v6) address.
 */
static bool isip(const char *domain)
{
  struct in_addr addr;
#ifdef ENABLE_IPV6
  struct in6_addr addr6;
#endif

  if(inet_pton(AF_INET, domain, &addr)
#ifdef ENABLE_IPV6
     || inet_pton(AF_INET6, domain, &addr6)
#endif
    ) {
    /* domain name given as IP address */
    return true;
  }

  return false;
}

/*
 * matching cookie path and url path
 * RFC6265 5.1.4 Paths and Path-Match
 */
static bool pathmatch(const char *cookie_path, const char *request_uri)
{
  size_t cookie_path_len;
  size_t uri_path_len;
  char *uri_path = NULL;
  char *pos;
  bool ret = false;

  /* cookie_path must not have last '/' separator. ex: /sample */
  cookie_path_len = strlen(cookie_path);
  if(1 == cookie_path_len) {
    /* cookie_path must be '/' */
    return true;
  }

  uri_path = strdup(request_uri);
  if(!uri_path)
    return false;
  pos = strchr(uri_path, '?');
  if(pos)
    *pos = 0x0;

  /* #-fragments are already cut off! */
  if(0 == strlen(uri_path) || uri_path[0] != '/') {
    free(uri_path);
    uri_path = strdup("/");
    if(!uri_path)
      return false;
  }

  /* here, RFC6265 5.1.4 says
     4. Output the characters of the uri-path from the first character up
        to, but not including, the right-most %x2F ("/").
     but URL path /hoge?fuga=xxx means /hoge/index.cgi?fuga=xxx in some site
     without redirect.
     Ignore this algorithm because /hoge is uri path for this case
     (uri path is not /).
   */

  uri_path_len = strlen(uri_path);

  if(uri_path_len < cookie_path_len) {
    ret = false;
    goto pathmatched;
  }

  /* not using checkprefix() because matching should be case-sensitive */
  if(strncmp(cookie_path, uri_path, cookie_path_len)) {
    ret = false;
    goto pathmatched;
  }

  /* The cookie-path and the uri-path are identical. */
  if(cookie_path_len == uri_path_len) {
    ret = true;
    goto pathmatched;
  }

  /* here, cookie_path_len < url_path_len */
  if(uri_path[cookie_path_len] == '/') {
    ret = true;
    goto pathmatched;
  }

  ret = false;

pathmatched:
  free(uri_path);
  return ret;
}

/* sort this so that the longest path gets before the shorter path */
static int cookie_sort(const void *p1, const void *p2)
{
  struct Cookie *c1 = *(struct Cookie **)p1;
  struct Cookie *c2 = *(struct Cookie **)p2;
  size_t l1, l2;

  /* 1 - compare cookie path lengths */
  l1 = c1->path ? strlen(c1->path) : 0;
  l2 = c2->path ? strlen(c2->path) : 0;

  if(l1 != l2)
    return (l2 > l1) ? 1 : -1 ; /* avoid size_t <=> int conversions */

  /* 2 - compare cookie domain lengths */
  l1 = c1->domain ? strlen(c1->domain) : 0;
  l2 = c2->domain ? strlen(c2->domain) : 0;

  if(l1 != l2)
    return (l2 > l1) ? 1 : -1 ;  /* avoid size_t <=> int conversions */

  /* 3 - compare cookie names */
  if(c1->name && c2->name)
    return strcmp(c1->name, c2->name);

  /* sorry, can't be more deterministic */
  return 0;
}

#define CLONE(field)                     \
  do {                                   \
    if(src->field) {                     \
      d->field = strdup(src->field);     \
      if(!d->field)                      \
        goto fail;                       \
    }                                    \
  } while(0)
static struct Cookie *dup_cookie(struct Cookie *src)
{
  struct Cookie *d = calloc(sizeof(struct Cookie), 1);
  if(d) {
    CLONE(expirestr);
    CLONE(domain);
    CLONE(path);
    CLONE(spath);
    CLONE(name);
    CLONE(value);
    CLONE(maxage);
    CLONE(version);
    d->expires = src->expires;
    d->tailmatch = src->tailmatch;
    d->secure = src->secure;
    d->httponly = src->httponly;
  }
  return d;

  fail:
  freecookie(d);
  return NULL;
}

static bool tailmatch(const char *cooke_domain, const char *hostname)
{
  size_t cookie_domain_len = strlen(cooke_domain);
  size_t hostname_len = strlen(hostname);

  if(hostname_len < cookie_domain_len)
    return false;

  if(!strcasecompare(cooke_domain, hostname+hostname_len-cookie_domain_len))
    return false;

  /* A lead char of cookie_domain is not '.'.
     RFC6265 4.1.2.3. The Domain Attribute says:
       For example, if the value of the Domain attribute is
       "example.com", the user agent will include the cookie in the Cookie
       header when making HTTP requests to example.com, www.example.com, and
       www.corp.example.com.
   */
  if(hostname_len == cookie_domain_len)
    return true;
  if('.' == *(hostname + hostname_len - cookie_domain_len - 1))
    return true;
  return false;
}

struct Cookie *cookie_getlist(struct CookieInfo *c,
                                   const char *host, const char *path,
                                   bool secure)
{
  struct Cookie *newco;
  struct Cookie *co;
  time_t now = time(NULL);
  struct Cookie *mainco=NULL;
  size_t matches = 0;
  bool is_ip;

  if(!c || !c->cookies)
    return NULL; /* no cookie struct or no cookies in the struct */

  /* at first, remove expired cookies */
  remove_expired(c);

  /* check if host is an IP(v4|v6) address */
  is_ip = isip(host);

  co = c->cookies;

  while(co) {
    /* only process this cookie if it is not expired or had no expire
       date AND that if the cookie requires we're secure we must only
       continue if we are! */
    if((!co->expires || (co->expires > now)) &&
       (co->secure?secure:true)) {

      /* now check if the domain is correct */
      if(!co->domain ||
         (co->tailmatch && !is_ip && tailmatch(co->domain, host)) ||
         ((!co->tailmatch || is_ip) && strcasecompare(host, co->domain)) ) {
        /* the right part of the host matches the domain stuff in the
           cookie data */

        /* now check the left part of the path with the cookies path
           requirement */
        if(!co->spath || pathmatch(co->spath, path) ) {

          /* and now, we know this is a match and we should create an
             entry for the return-linked-list */

          newco = dup_cookie(co);
          if(newco) {
            /* then modify our next */
            newco->next = mainco;

            /* point the main to us */
            mainco = newco;

            matches++;
          }
          else {
            fail:
            /* failure, clear up the allocated chain and return NULL */
            cookie_freelist(mainco);
            return NULL;
          }
        }
      }
    }
    co = co->next;
  }

  if(matches) {
    /* Now we need to make sure that if there is a name appearing more than
       once, the longest specified path version comes first. To make this
       the swiftest way, we just sort them all based on path length. */
    struct Cookie **array;
    size_t i;

    /* alloc an array and store all cookie pointers */
    array = malloc(sizeof(struct Cookie *) * matches);
    if(!array)
      goto fail;

    co = mainco;

    for(i=0; co; co = co->next)
      array[i++] = co;

    /* now sort the cookie pointers in path length order */
    qsort(array, matches, sizeof(struct Cookie *), cookie_sort);

    /* remake the linked list order according to the new order */

    mainco = array[0]; /* start here */
    for(i=0; i<matches-1; i++)
      array[i]->next = array[i+1];
    array[matches-1]->next = NULL; /* terminate the list */

    free(array); /* remove the temporary data again */
  }

  return mainco; /* return the new list */
}

struct CookieInfo *cookie_loadfile(const char *cookie_file)
{
	struct CookieInfo *c;
	FILE *fp = NULL;
	char *line = NULL;

	c = calloc(1, sizeof(struct CookieInfo));
	if(!c)
		return NULL; /* failed to get memory */

	fp = fopen(cookie_file, "r");

	if(!fp) {
		goto fail;
	}

	if(fp) {
		char *lineptr;

		line = malloc(MAX_COOKIE_LINE);
		if(!line)
			goto fail;
		while(get_line(line, MAX_COOKIE_LINE, fp)) {
			lineptr=line;
			while(*lineptr && ISBLANK(*lineptr))
				lineptr++;

			cookie_add(c, lineptr);
		}
		free(line); /* free the line buffer */

		fclose(fp);
	}
	return c;

fail:
	cookie_cleanup(c);
	if(line)
		free(line);
	if(fp)
		fclose(fp);
	return NULL; /* out of memory */
}

void cookie_cleanup(struct CookieInfo *c)
{
	if(c) {
		cookie_freelist(c->cookies);
		free(c); /* free the base struct as well */
	}
}

void cookie_freelist(struct Cookie *co)
{
	struct Cookie *next;
	while(co) {
		next = co->next;
		freecookie(co);
		co = next;
	}
}

struct Cookie *cookie_add(struct CookieInfo *c, char *lineptr)
{
	struct Cookie *clist;
	struct Cookie *co;
	struct Cookie *lastc = NULL;
	bool replace_old = false;
	bool badcookie = false; /* cookies are good by default. mmmmm yummy */
	char *ptr;
	char *firstptr;
	char *tok_buf = NULL;
	int fields;

	/* First, alloc and init a new struct for it */
	co = calloc(1, sizeof(struct Cookie));
	if (!co) return NULL; /* bail out if we're this low on memory */

	/* IE introduced HTTP-only cookies to prevent XSS attacks. Cookies
	   marked with httpOnly after the domain name are not accessible
	   from javascripts, but since curl does not operate at javascript
	   level, we include them anyway. In Firefox's cookie files, these
	   lines are preceded with #HttpOnly_ and then everything is
	   as usual, so we skip 10 characters of the line..
	   */
	if (strncmp(lineptr, "#HttpOnly_", 10) == 0) {
		lineptr += 10;
		co->httponly = true;
	}

	if (lineptr[0] == '#') {
		/* don't even try the comments */
		free(co);
		return NULL;
	}
	/* strip off the possible end-of-line characters */
	ptr = strchr(lineptr, '\r');
	if (ptr) *ptr = 0; /* clear it */
	ptr = strchr(lineptr, '\n');
	if (ptr) *ptr = 0; /* clear it */

	firstptr = strtok_r(lineptr, "\t", &tok_buf); /* tokenize it on the TAB */

	/* Now loop through the fields and init the struct we already have
	   allocated */
	for (ptr = firstptr, fields = 0; ptr && !badcookie;
	     ptr = strtok_r(NULL, "\t", &tok_buf), fields++) {
		switch (fields) {
			case 0:
				if (ptr[0] == '.') /* skip preceding dots */
					ptr++;
				co->domain = strdup(ptr);
				if (!co->domain) badcookie = true;
				break;
			case 1:
				/* This field got its explanation on the 23rd of May 2001 by
				   Andrés García:

flag: A true/false value indicating if all machines within a given
domain can access the variable. This value is set automatically by
the browser, depending on the value you set for the domain.

As far as I can see, it is set to true when the cookie says
.domain.com and to false when the domain is complete www.domain.com
*/
				co->tailmatch = strcasecompare(ptr, "true") ? true : false;
				break;
			case 2:
				/* It turns out, that sometimes the file format allows the path
				   field to remain not filled in, we try to detect this and work
				   around it! Andrés García made us aware of this... */
				if (strcmp("true", ptr) && strcmp("false", ptr)) {
					/* only if the path doesn't look like a boolean option! */
					co->path = strdup(ptr);
					if (!co->path)
						badcookie = true;
					else {
						co->spath = sanitize_cookie_path(co->path);
						if (!co->spath) {
							badcookie = true; /* out of memory bad */
						}
					}
					break;
				}
				/* this doesn't look like a path, make one up! */
				co->path = strdup("/");
				if (!co->path) badcookie = true;
				co->spath = strdup("/");
				if (!co->spath) badcookie = true;
				fields++; /* add a field and fall down to secure */
				/* FALLTHROUGH */
			case 3:
				co->secure = strcasecompare(ptr, "true") ? true : false;
				break;
			case 4:
				co->expires = strtol(ptr, NULL, 10);
				break;
			case 5:
				co->name = strdup(ptr);
				if (!co->name) badcookie = true;
				break;
			case 6:
				co->value = strdup(ptr);
				if (!co->value) badcookie = true;
				break;
		}
	}
	if (6 == fields) {
		/* we got a cookie with blank contents, fix it */
		co->value = strdup("");
		if (!co->value)
			badcookie = true;
		else
			fields++;
	}

	if (!badcookie && (7 != fields))
		/* we did not find the sufficient number of fields */
		badcookie = true;

	if (badcookie) {
		freecookie(co);
		return NULL;
	}

	/* now, we have parsed the incoming line, we must now check if this
	   superceeds an already existing cookie, which it may if the previous have
	   the same domain and path as this */

	/* at first, remove expired cookies */
	remove_expired(c);

	clist = c->cookies;
	replace_old = false;
	while (clist) {
		if (strcasecompare(clist->name, co->name)) {
			/* the names are identical */

			if (clist->domain && co->domain) {
				if (strcasecompare(clist->domain, co->domain) &&
				    (clist->tailmatch == co->tailmatch))
					/* The domains are identical */
					replace_old = true;
			} else if (!clist->domain && !co->domain)
				replace_old = true;

			if (replace_old) {
				/* the domains were identical */

				if (clist->spath && co->spath) {
					if (strcasecompare(clist->spath, co->spath)) {
						replace_old = true;
					} else
						replace_old = false;
				} else if (!clist->spath && !co->spath)
					replace_old = true;
				else
					replace_old = false;
			}

			if (replace_old) {
				co->next = clist->next; /* get the next-pointer first */

				/* then free all the old pointers */
				free(clist->name);
				free(clist->value);
				free(clist->domain);
				free(clist->path);
				free(clist->spath);
				free(clist->expirestr);
				free(clist->version);
				free(clist->maxage);

				*clist = *co; /* then store all the new data */

				free(co);   /* free the newly alloced memory */
				co = clist; /* point to the previous struct instead */

				/* We have replaced a cookie, now skip the rest of the list but
				   make sure the 'lastc' pointer is properly set */
				do {
					lastc = clist;
					clist = clist->next;
				} while (clist);
				break;
			}
		}
		lastc = clist;
		clist = clist->next;
	}

	if (!replace_old) {
		/* then make the last item point on this new one */
		if (lastc)
			lastc->next = co;
		else
			c->cookies = co;
		c->numcookies++; /* one more cookie in the jar */
	}

	return co;
}

#endif /* !GIT_WINHTTP */
