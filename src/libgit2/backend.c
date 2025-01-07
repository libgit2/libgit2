/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include "array.h"
#include "backend.h"
#include "runtime.h"

/* Number of features in git_feature_t. */
#define FEATURE_TABLE_SIZE 12

typedef struct git__backend {
	/* Name of this backend. */
	const char *name;

	/* Called when the backend becomes active. */
	git_backend__setup_cb install;

	/* Called when an active backend becomes inactive. */
	git_backend__setup_cb uninstall;

	/* User data passed to install/uninstall. */
	void *payload;
} git__backend_t;

typedef struct git__feature_backends {
	/* Available backends for this feature. */
	git_array_t(git__backend_t) backends;

	/* Currently active backend for this feature. */
	git__backend_t *active_backend;

	/* Default backend for this feature. */
	git__backend_t *default_backend;

	/* Comma-separated list of backend names that can be used for this feature. */
	git_str spec;

	/* Is the spec string immutable? */
	bool spec_frozen;
} git__feature_backends_t;

/* Table of backends for every feature. */
static git__feature_backends_t feature_table[FEATURE_TABLE_SIZE];

GIT_INLINE(git__feature_backends_t *) get_feature_backends(git_feature_t feature_mask)
{
	int feature_index = git__bsr(feature_mask);

	if ((size_t)feature_index >= ARRAY_SIZE(feature_table))
		return NULL;

	return &feature_table[feature_index];
}

static void git_backend__shutdown(void)
{
	size_t i;
	git__feature_backends_t *feature_backends;

	for (i = 0; i < ARRAY_SIZE(feature_table); i++) {
		feature_backends = &feature_table[i];
		git_array_dispose(feature_backends->backends);
		git_str_dispose(&feature_backends->spec);
	}

	memset(feature_table, 0, sizeof(feature_table));
}

int git_backend_global_init(void)
{
	size_t i;
	git__feature_backends_t *feature_backends;

	memset(feature_table, 0, sizeof(feature_table));

	for (i = 0; i < ARRAY_SIZE(feature_table); i++) {
		feature_backends = &feature_table[i];
		git_array_init(feature_backends->backends);
		git_str_init(&feature_backends->spec, 0);
	}

	return git_runtime_shutdown_register(git_backend__shutdown);
}

int git_backend__register(
	git_feature_t feature,
	const char *name,
	git_backend__setup_cb install,
	git_backend__setup_cb uninstall,
	void *payload)
{
	int error = 0;
	size_t i;
	git__backend_t *backend;
	git__feature_backends_t *feature_backends;

	feature_backends = get_feature_backends(feature);
	if (!feature_backends)
		GIT_ASSERT(!"increase FEATURE_TABLE_SIZE to accomodate your new feature");

	/* Prevent duplicate backend names */
	git_array_foreach(feature_backends->backends, i, backend)
		GIT_ASSERT(strcmp(name, backend->name));

	/* Initialize new backend */
	backend = git_array_alloc(feature_backends->backends);
	GIT_ERROR_CHECK_ALLOC(backend);
	memset(backend, 0, sizeof(*backend));
	backend->name      = name;
	backend->install   = install;
	backend->uninstall = uninstall;
	backend->payload   = payload;

	/* Update spec string (comma-separated list of available backends) */
	if (feature_backends->spec_frozen)
		GIT_ASSERT(!"DON'T register new backends after querying available backends!");
	if (feature_backends->spec.size > 0) {
		error = git_str_putc(&feature_backends->spec, ',');
		GIT_ERROR_CHECK_ERROR(error);
	}
	error = git_str_puts(&feature_backends->spec, backend->name);
	GIT_ERROR_CHECK_ERROR(error);

	/* Set first backend for this feature as default */
	if (!feature_backends->default_backend) {
		feature_backends->default_backend = backend;
		error = git_backend__set(feature, name);
		GIT_ERROR_CHECK_ERROR(error);
	}

	return error;
}

int git_backend__set(git_feature_t feature, const char *name)
{
	size_t i;
	git__backend_t *candidate;
	git__backend_t *new_backend = NULL;
	git__backend_t *old_backend;
	git__feature_backends_t *feature_backends;

	feature_backends = get_feature_backends(feature);
	GIT_ASSERT_WITH_RETVAL(feature_backends, GIT_EINVALID);

	if (name == NULL) {
		/* Restore default backend */
		new_backend = feature_backends->default_backend;
	} else if (!name[0]) {
		/* Empty string, clear backend */
		GIT_ASSERT(!new_backend);
	} else {
		/* Search for backend by name */
		git_array_foreach(feature_backends->backends, i, candidate) {
			if (!strcmp(name, candidate->name)) {
				new_backend = candidate;
				break;
			}
		}

		/* Not found */
		if (!new_backend) {
			git_error_set(GIT_ERROR_INVALID, "backend '%s' not built for feature %d", name, feature);
			return GIT_ENOTFOUND;
		}
	}

	/* Change backends */
	old_backend = feature_backends->active_backend;
	if (old_backend != new_backend) {
		if (old_backend && old_backend->uninstall) {
			old_backend->uninstall(old_backend->payload);
		}

		feature_backends->active_backend = new_backend;

		if (new_backend && new_backend->install) {
			new_backend->install(new_backend->payload);
		}
	}

	return 0;
}

const char *git_backend__name(git_feature_t feature)
{
	git__backend_t *backend;
	git__feature_backends_t *feature_backends = get_feature_backends(feature);
	GIT_ASSERT_WITH_RETVAL(feature_backends, NULL);

	if (feature_backends->backends.size == 0) {
		/* We may get here if the user queries GIT_OPT_GET_BACKEND for
		 * a feature that doesn't support changeable backends. */
		return git_libgit2_feature_backend(feature);
	}

	backend = feature_backends->active_backend;
	return backend ? backend->name : NULL;
}

const char *git_backend__spec(git_feature_t feature)
{
	git__feature_backends_t *feature_backends = get_feature_backends(feature);
	GIT_ASSERT_WITH_RETVAL(feature_backends, NULL);

	/* Once initialized, the spec is immutable because
	 * git_libgit2_feature_backend() returns a const char *. */
	feature_backends->spec_frozen = true;

	if (feature_backends->spec.size == 0) {
		/* In keeping with git_libgit2_feature_backend(), return NULL
		 * if no backends are available for this feature. */
		return NULL;
	}

	return feature_backends->spec.ptr;
}
