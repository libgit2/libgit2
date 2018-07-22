/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "warning.h"

#include "global.h"

git_vector git_warning__registration = GIT_VECTOR_INIT;

typedef struct {
	git_warning_token *token;
	int16_t mask;
	git_warning_cb cb;
	void *payload;
} git_warning_registration;

static int build_warning(git_warning *warn, git_warning_class klass, git_repository *repo, void *context, const char *msg, va_list args)
{
	git_buf msg_buf = GIT_BUF_INIT;
	warn->klass = klass;
	warn->repo = repo;
	warn->context = context;

	git_buf_vprintf(&msg_buf, msg, args);
	warn->msg = git_buf_detach(&msg_buf);

	return 0;
}

void git_warn__dispose(git_warning *warn)
{
	git__free(warn->msg);
}

static int notify_registrations(int *reply, git_warning *warn, size_t start_pos)
{
	while (start_pos < git_vector_length(&git_warning__registration)) {
		git_warning_registration *reg = git_vector_get(&git_warning__registration, start_pos++);
		if (GIT_WARNING_TYPE(reg->mask) != GIT_WARNING_ANY &&
		    GIT_WARNING_TYPE(reg->mask) != GIT_WARNING_TYPE(warn->klass)) {
			/* gone over that type registrations */
			return GIT_ENOTFOUND;
		}
		if (GIT_WARNING_CODE(reg->mask) == GIT_WARNING_ANY ||
		    (GIT_WARNING_CODE(reg->mask) & GIT_WARNING_CODE(warn->klass)) != 0) {
			/* matching warning, call */
			int cb_reply = reg->cb(warn, reg->payload);
			if (cb_reply != GIT_PASSTHROUGH) {
				*reply = cb_reply;
				return 0;
			}
		}
	}
	return GIT_ENOTFOUND;
}

int git_warn__raise(git_warning_class klass, git_repository *repo, void *context, const char *msg, ...)
{
	git_warning warn;
	va_list args;
	int reply = GIT_PASSTHROUGH;
	size_t pos;
	git_warning_registration key;

	va_start(args, msg);
	build_warning(&warn, klass, repo, context, msg, args);
	va_end(args);

	/* First check for registrations with a specified type */
	memset(&key, 0, sizeof(key));
	key.mask = GIT_WARNING_CLASS(GIT_WARNING_TYPE(klass), GIT_WARNING_ANY);
	if (git_vector_bsearch(&pos, &git_warning__registration, &key) == 0) {
		if ((notify_registrations(&reply, &warn, pos)) < 0) {
			/* We got a non-passthrough answer, we're done */
			goto cleanup;
		}
	}

	/* Now check for "any" callbacks */
	key.mask = GIT_WARNING_CLASS(GIT_WARNING_ANY, GIT_WARNING_ANY);
	if (git_vector_bsearch(&pos, &git_warning__registration, &key) == 0) {
		if ((notify_registrations(&reply, &warn, pos)) < 0) {
			/* We got a non-passthrough answer, we're done */
			goto cleanup;
		}
	}

cleanup:
	git_warn__dispose(&warn);

	return reply;
}

static int warn_registration_cmp(const void *a, const void *b)
{
	const git_warning_registration *reg_a = a;
	const git_warning_registration *reg_b = b;

	if (GIT_WARNING_TYPE(reg_a->mask) == GIT_WARNING_ANY &&
	    GIT_WARNING_TYPE(reg_b->mask) == GIT_WARNING_ANY)
		return 0;

	if (GIT_WARNING_TYPE(reg_a->mask) == GIT_WARNING_ANY)
		return -1;

	if (GIT_WARNING_TYPE(reg_b->mask) == GIT_WARNING_ANY)
		return 1;

	if (reg_a->mask != reg_b->mask)
		return reg_a->mask - reg_b->mask;

	return 0;

//	if (GIT_WARNING_TYPE(reg_a->mask) != GIT_WARNING_TYPE(reg_b->mask))
//		return reg_a->mask - reg_b->mask;
//
//	if ((GIT_WARNING_CODE(reg_a->mask) == GIT_WARNING_ANY) ||
//	    (GIT_WARNING_CODE(reg_b->mask) != GIT_WARNING_ANY))
//		return -1;
//
//	if ((GIT_WARNING_CODE(reg_a->mask) != GIT_WARNING_ANY) ||
//	    (GIT_WARNING_CODE(reg_b->mask) == GIT_WARNING_ANY))
//		return 1;
//
//	if (GIT_WARNING_CODE(reg_a->mask) != GIT_WARNING_CODE(reg_b->mask))
//		return reg_a->mask - reg_b->mask;
//
//	return 0;
}

int git_warning_register(git_warning_token *token, int16_t mask, git_warning_cb cb, void *payload)
{
	git_warning_registration *reg;

	reg = git__malloc(sizeof(*reg));
	reg->token = token;
	reg->mask = mask;
	reg->cb = cb;
	reg->payload = payload;

	return git_vector_insert_sorted(&git_warning__registration, reg, NULL);
}

int git_warning_unregister(git_warning_token *token)
{
	int res = GIT_ENOTFOUND;
	int pos;

	/* we iterate in reverse because it's likely the registration is recent */
	for (pos = git_vector_length(&git_warning__registration) - 1; pos >= 0; pos--) {
		git_warning_registration *reg = git_vector_get(&git_warning__registration, pos);
		if (reg->token != token)
			continue;

		git_vector_remove(&git_warning__registration, pos);
		res = GIT_OK;
	}

	return res;
}

int git_warning_global_init(void)
{
	git_vector_init(&git_warning__registration, 1, warn_registration_cmp);

	git__on_shutdown(git_warning_global_shutdown);

	return 0;
}

void git_warning_global_shutdown(void)
{
	git_vector_free_deep(&git_warning__registration);
}
