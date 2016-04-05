/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "array.h"

#include "git2/cancellable.h"

typedef struct {
	git_cancellable_cb cb;
	void *payload;
} registration;

struct  git_cancellable {
	git_array_t(registration) registrations;
	git_atomic cancelled;
};

struct git_cancellable_source {
	git_cancellable token;
};


int git_cancellable_source_new(git_cancellable_source **out)
{
	git_cancellable_source *cs;

	cs = git__calloc(1, sizeof(git_cancellable_source));
	GITERR_CHECK_ALLOC(cs);

	*out= cs;
	return 0;
}

int git_cancellable_source_free(git_cancellable_source *cs)
{
	git_array_clear(cs->token.registrations);
	git__free(cs);
}

int git_cancellable_is_cancelled(git_cancellable *c)
{
	return c->cancelled.val;
}

int git_cancellable_register(git_cancellable *c, git_cancellable_cb cb, void *payload)
{
	registration *reg = git_array_alloc(c->registrations);
	GITERR_CHECK_ALLOC(reg);

	reg->cb = cb;
	reg->payload = payload;

	return 0;
}

git_cancellable *git_cancellable_source_token(git_cancellable_source *cs)
{
	return &cs->token;
}

void git_cancellable_source_cancel(git_cancellable_source *cs)
{
	size_t i;
	registration *reg;

	git_atomic_set(&cs->token.cancelled, 1);

	for (i = 0; i < git_array_size(cs->token.registrations); i++) {
		reg = git_array_get(cs->token.registrations, i);
		assert(reg);
		reg->cb(&cs->token, reg->payload);
	}
}
