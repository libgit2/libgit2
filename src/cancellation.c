/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "global.h"
#include "array.h"

#include "cancellation.h"

typedef struct {
	git_cancellation_cb cb;
	void *payload;
} registration;

struct git_cancellation {
	/* Lock over the whole structure */
	git_mutex lock;
	git_array_t(registration) registrations;
	bool cancelled;
};

int git_cancellation_new(git_cancellation **out)
{
	git_cancellation *c;

	c = git__calloc(1, sizeof(git_cancellation));
	GITERR_CHECK_ALLOC(c);

	if (git_mutex_init(&c->lock) < 0) {
		git__free(c);
		giterr_set(GITERR_OS, "failed to initialize cancellation lock");
		return -1;
	}

	*out = c;
	return 0;
}

void git_cancellation_free(git_cancellation *c)
{
	if (!c)
		return;

	git_array_clear(c->registrations);
	git__free(c);
}

int git_cancellation_requested(git_cancellation *c)
{
	/* Should we have a memory barrier here? a full lock? */
	return c->cancelled;
}

int git_cancellation_register(git_cancellation *c, git_cancellation_cb cb, void *payload)
{
	registration *reg;
	int error = 0;

	/* If we've already cancelled we'll never fire; no-op it */
	if (c->cancelled)
		return 0;

	if (git_mutex_lock(&c->lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock cancellation token");
		return -1;
	}

	/* Cancelled, but we lost the lock race */
	if (c->cancelled)
		goto unlock;

	reg = git_array_alloc(c->registrations);
	if (!reg) {
		giterr_set_oom();
		error = -1;
		goto unlock;
	}

	reg->cb = cb;
	reg->payload = payload;

 unlock:
	git_mutex_unlock(&c->lock);
	return error;
}

int git_cancellation_request(git_cancellation *c)
{
	size_t i;
	int error = 0;
	registration *reg;

	/* You can only cancel once, short-circuit when already done */
	if (c->cancelled)
		return 0;

	if (git_mutex_lock(&c->lock) < 0) {
		giterr_set(GITERR_OS, "failed to lock cancellation token");
		return -1;
	}

	/* We lost the race for the mutex */
	if (c->cancelled)
		goto unlock;

	c->cancelled = true;

	/* Run the registered hooks if it's the first cancellation */
	for (i = 0; i < git_array_size(c->registrations); i++) {
		reg = git_array_get(c->registrations, i);
		assert(reg);
		if ((error = reg->cb(c, reg->payload)) < 0)
			goto unlock;
	}

 unlock:
	git_mutex_unlock(&c->lock);
	return error;
}

int git_cancellation_activate(git_cancellation *c)
{
	git_cancellation *old;

	old = git__swap(GIT_GLOBAL->cancellation, c);

	git_cancellation_free(old);
	return 0;
}

int git_cancellation_deactivate(void)
{
	git_cancellation *old;

	old = git__swap(GIT_GLOBAL->cancellation, NULL);

	git_cancellation_free(old);
	return 0;
}
