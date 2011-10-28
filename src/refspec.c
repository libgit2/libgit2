/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "common.h"
#include "refspec.h"
#include "util.h"

int git_refspec_parse(git_refspec *refspec, const char *str)
{
	char *delim;

	memset(refspec, 0x0, sizeof(git_refspec));

	if (*str == '+') {
		refspec->force = 1;
		str++;
	}

	delim = strchr(str, ':');
	if (delim == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse refspec. No ':'");

	refspec->src = git__strndup(str, delim - str);
	if (refspec->src == NULL)
		return GIT_ENOMEM;

	refspec->dst = git__strdup(delim + 1);
	if (refspec->dst == NULL) {
		git__free(refspec->src);
		refspec->src = NULL;
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

const char *git_refspec_src(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->src;
}

const char *git_refspec_dst(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->dst;
}

int git_refspec_src_match(const git_refspec *refspec, const char *refname)
{
	return refspec == NULL ? GIT_ENOMATCH : git__fnmatch(refspec->src, refname, 0);
}

int git_refspec_transform(char *out, size_t outlen, const git_refspec *spec, const char *name)
{
	size_t baselen, namelen;

	baselen = strlen(spec->dst);
	if (outlen <= baselen)
		return git__throw(GIT_EINVALIDREFNAME, "Reference name too long");

	/*
	 * No '*' at the end means that it's mapped to one specific local
	 * branch, so no actual transformation is needed.
	 */
	if (spec->dst[baselen - 1] != '*') {
		memcpy(out, spec->dst, baselen + 1); /* include '\0' */
		return GIT_SUCCESS;
	}

	/* There's a '*' at the end, so remove its length */
	baselen--;

	/* skip the prefix, -1 is for the '*' */
	name += strlen(spec->src) - 1;

	namelen = strlen(name);

	if (outlen <= baselen + namelen)
		return git__throw(GIT_EINVALIDREFNAME, "Reference name too long");

	memcpy(out, spec->dst, baselen);
	memcpy(out + baselen, name, namelen + 1);

	return GIT_SUCCESS;
}
