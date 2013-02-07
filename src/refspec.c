/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2/errors.h"

#include "common.h"
#include "refspec.h"
#include "util.h"
#include "posix.h"
#include "refs.h"

int git_refspec__parse(git_refspec *refspec, const char *input, bool is_fetch)
{
	// Ported from https://github.com/git/git/blob/f06d47e7e0d9db709ee204ed13a8a7486149f494/remote.c#L518-636

	size_t llen;
	int is_glob = 0;
	const char *lhs, *rhs;
	int flags;

	assert(refspec && input);

	memset(refspec, 0x0, sizeof(git_refspec));

	lhs = input;
	if (*lhs == '+') {
		refspec->force = 1;
		lhs++;
	}

	rhs = strrchr(lhs, ':');

	/*
	 * Before going on, special case ":" (or "+:") as a refspec
	 * for matching refs.
	 */
	if (!is_fetch && rhs == lhs && rhs[1] == '\0') {
		refspec->matching = 1;
		return 0;
	}

	if (rhs) {
		size_t rlen = strlen(++rhs);
		is_glob = (1 <= rlen && strchr(rhs, '*'));
		refspec->dst = git__strndup(rhs, rlen);
	}

	llen = (rhs ? (size_t)(rhs - lhs - 1) : strlen(lhs));
	if (1 <= llen && memchr(lhs, '*', llen)) {
		if ((rhs && !is_glob) || (!rhs && is_fetch))
			goto invalid;
		is_glob = 1;
	} else if (rhs && is_glob)
		goto invalid;

	refspec->pattern = is_glob;
	refspec->src = git__strndup(lhs, llen);
	flags = GIT_REF_FORMAT_ALLOW_ONELEVEL
		| (is_glob ? GIT_REF_FORMAT_REFSPEC_PATTERN : 0);

	if (is_fetch) {
		/*
			* LHS
			* - empty is allowed; it means HEAD.
			* - otherwise it must be a valid looking ref.
			*/
		if (!*refspec->src)
			; /* empty is ok */
		else if (!git_reference__is_valid_name(refspec->src, flags))
			goto invalid;
		/*
			* RHS
			* - missing is ok, and is same as empty.
			* - empty is ok; it means not to store.
			* - otherwise it must be a valid looking ref.
			*/
		if (!refspec->dst)
			; /* ok */
		else if (!*refspec->dst)
			; /* ok */
		else if (!git_reference__is_valid_name(refspec->dst, flags))
			goto invalid;
	} else {
		/*
			* LHS
			* - empty is allowed; it means delete.
			* - when wildcarded, it must be a valid looking ref.
			* - otherwise, it must be an extended SHA-1, but
			*   there is no existing way to validate this.
			*/
		if (!*refspec->src)
			; /* empty is ok */
		else if (is_glob) {
			if (!git_reference__is_valid_name(refspec->src, flags))
				goto invalid;
		}
		else {
			; /* anything goes, for now */
		}
		/*
			* RHS
			* - missing is allowed, but LHS then must be a
			*   valid looking ref.
			* - empty is not allowed.
			* - otherwise it must be a valid looking ref.
			*/
		if (!refspec->dst) {
			if (!git_reference__is_valid_name(refspec->src, flags))
				goto invalid;
		} else if (!*refspec->dst) {
			goto invalid;
		} else {
			if (!git_reference__is_valid_name(refspec->dst, flags))
				goto invalid;
		}
	}

	return 0;

 invalid:
	return -1;
}

void git_refspec__free(git_refspec *refspec)
{
	if (refspec == NULL)
		return;

	git__free(refspec->src);
	git__free(refspec->dst);
}

const char *git_refspec_src(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->src;
}

const char *git_refspec_dst(const git_refspec *refspec)
{
	return refspec == NULL ? NULL : refspec->dst;
}

int git_refspec_force(const git_refspec *refspec)
{
	assert(refspec);

	return refspec->force;
}

int git_refspec_src_matches(const git_refspec *refspec, const char *refname)
{
	if (refspec == NULL || refspec->src == NULL)
		return false;

	return (p_fnmatch(refspec->src, refname, 0) == 0);
}

int git_refspec_dst_matches(const git_refspec *refspec, const char *refname)
{
	if (refspec == NULL || refspec->dst == NULL)
		return false;

	return (p_fnmatch(refspec->dst, refname, 0) == 0);
}

static int refspec_transform_internal(char *out, size_t outlen, const char *from, const char *to, const char *name)
{
	size_t baselen, namelen;

	baselen = strlen(to);
	if (outlen <= baselen) {
		giterr_set(GITERR_INVALID, "Reference name too long");
		return GIT_EBUFS;
	}

	/*
	 * No '*' at the end means that it's mapped to one specific local
	 * branch, so no actual transformation is needed.
	 */
	if (to[baselen - 1] != '*') {
		memcpy(out, to, baselen + 1); /* include '\0' */
		return 0;
	}

	/* There's a '*' at the end, so remove its length */
	baselen--;

	/* skip the prefix, -1 is for the '*' */
	name += strlen(from) - 1;

	namelen = strlen(name);

	if (outlen <= baselen + namelen) {
		giterr_set(GITERR_INVALID, "Reference name too long");
		return GIT_EBUFS;
	}

	memcpy(out, to, baselen);
	memcpy(out + baselen, name, namelen + 1);

	return 0;
}

int git_refspec_transform(char *out, size_t outlen, const git_refspec *spec, const char *name)
{
	return refspec_transform_internal(out, outlen, spec->src, spec->dst, name);
}

int git_refspec_rtransform(char *out, size_t outlen, const git_refspec *spec, const char *name)
{
	return refspec_transform_internal(out, outlen, spec->dst, spec->src, name);
}

static int refspec_transform(git_buf *out, const char *from, const char *to, const char *name)
{
	if (git_buf_sets(out, to) < 0)
		return -1;

	/*
	 * No '*' at the end means that it's mapped to one specific
	 * branch, so no actual transformation is needed.
	 */
	if (git_buf_len(out) > 0 && out->ptr[git_buf_len(out) - 1] != '*')
		return 0;

	git_buf_truncate(out, git_buf_len(out) - 1); /* remove trailing '*' */
	git_buf_puts(out, name + strlen(from) - 1);

	if (git_buf_oom(out))
		return -1;

	return 0;
}

int git_refspec_transform_r(git_buf *out, const git_refspec *spec, const char *name)
{
	return refspec_transform(out, spec->src, spec->dst, name);
}

int git_refspec_transform_l(git_buf *out, const git_refspec *spec, const char *name)
{
	return refspec_transform(out, spec->dst, spec->src, name);
}

int git_refspec__serialize(git_buf *out, const git_refspec *refspec)
{
	if (refspec->force)
		git_buf_putc(out, '+');

	git_buf_printf(out, "%s:%s",
		refspec->src != NULL ? refspec->src : "",
		refspec->dst != NULL ? refspec->dst : "");

	return git_buf_oom(out) == false;
}

int git_refspec_is_wildcard(const git_refspec *spec)
{
	assert(spec && spec->src);

	return (spec->src[strlen(spec->src) - 1] == '*');
}
