/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "common.h"
#include "refspec.h"
#include "util.h"
#include "posix.h"

int git_refspec_parse(git_refspec *refspec, const char *str)
{
	char *delim;

	memset(refspec, 0x0, sizeof(git_refspec));

	if (*str == '+') {
		refspec->force = 1;
		str++;
	}

	delim = strchr(str, ':');
	if (delim == NULL) {
		refspec->src = git__strdup(str);
		GITERR_CHECK_ALLOC(refspec->src);
		return 0;
	}

	refspec->src = git__strndup(str, delim - str);
	GITERR_CHECK_ALLOC(refspec->src);

	refspec->dst = git__strdup(delim + 1);
	if (refspec->dst == NULL) {
		git__free(refspec->src);
		refspec->src = NULL;
		return -1;
	}

	return 0;
}

const char *git_refspec_src(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->src;
}

const char *git_refspec_dst(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->dst;
}

int git_refspec_src_matches(const git_refspec *refspec, const char *refname)
{
	if (refspec == NULL || refspec->src == NULL)
		return false;

	return (p_fnmatch(refspec->src, refname, 0) == 0);
}

int git_refspec_transform(char *out, size_t outlen, const git_refspec *spec, const char *name)
{
	size_t baselen, namelen;

	baselen = strlen(spec->dst);
	if (outlen <= baselen) {
		giterr_set(GITERR_INVALID, "Reference name too long");
		return GIT_EBUFS;
	}

	/*
	 * No '*' at the end means that it's mapped to one specific local
	 * branch, so no actual transformation is needed.
	 */
	if (spec->dst[baselen - 1] != '*') {
		memcpy(out, spec->dst, baselen + 1); /* include '\0' */
		return 0;
	}

	/* There's a '*' at the end, so remove its length */
	baselen--;

	/* skip the prefix, -1 is for the '*' */
	name += strlen(spec->src) - 1;

	namelen = strlen(name);

	if (outlen <= baselen + namelen) {
		giterr_set(GITERR_INVALID, "Reference name too long");
		return GIT_EBUFS;
	}

	memcpy(out, spec->dst, baselen);
	memcpy(out + baselen, name, namelen + 1);

	return 0;
}

int git_refspec_transform_r(git_buf *out, const git_refspec *spec, const char *name)
{
	if (git_buf_sets(out, spec->dst) < 0)
		return -1;

	/*
	 * No '*' at the end means that it's mapped to one specific local
	 * branch, so no actual transformation is needed.
	 */
	if (git_buf_len(out) > 0 && out->ptr[git_buf_len(out) - 1] != '*')
		return 0;

	git_buf_truncate(out, git_buf_len(out) - 1); /* remove trailing '*' */
	git_buf_puts(out, name + strlen(spec->src) - 1);

	if (git_buf_oom(out))
		return -1;

	return 0;
}

