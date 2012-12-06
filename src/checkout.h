/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_checkout_h__
#define INCLUDE_checkout_h__

#include "git2/checkout.h"
#include "iterator.h"

#define GIT_CHECKOUT__FREE_BASELINE (1u << 24)

/**
 * Given a working directory which is expected to match the contents
 * of iterator "expected", this will make the directory match the
 * contents of "desired" according to the rules in the checkout "opts".
 *
 * Because the iterators for the desired and expected values were already
 * created when this is invoked, if the checkout opts `paths` is in play,
 * then presumably the pathspec_pfx was already computed, so it should be
 * passed in to prevent reallocation.
 */
extern int git_checkout__from_iterators(
	git_iterator *desired,
	git_iterator *expected,
	git_checkout_opts *opts,
	const char *pathspec_pfx);

#endif
