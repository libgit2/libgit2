/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_backend_h__
#define INCLUDE_backend_h__

#include "common.h"

typedef int (*git_backend__setup_cb)(void *payload);

int git_backend_global_init(void);

/**
 * Get a comma-separated list of changeable backends supported by the given
 * feature.
 *
 * The first call to this function causes the spec to become immutable until
 * library shutdown. After calling this, new backends cannot be registered for
 * this feature.
 *
 * @param feature The feature.
 *
 * @return A string, or NULL if no backends are available for this feature.
 */
const char *git_backend__spec(git_feature_t feature);

/**
 * Get the name of the changeable backend that is currently active for the
 * given feature.
 *
 * @param feature The feature.
 *
 * @return Backend name. Empty string if no changeable backend is active for
 * this feature. NULL on error.
 */
const char *git_backend__name(git_feature_t feature);

/**
 * Change the backend for the given feature.
 * The feature must support changeable backends.
 *
 * @param feature The feature.
 * @param name The name of the backend. NULL resets the default backend.
 * Empty string disables the backend.
 *
 * @return 0 or an error code
 */
int git_backend__set(git_feature_t feature, const char *name);

/**
 * Register a changeable backend for the given feature.
 *
 * @param feature The feature.
 * @param name The name of the backend.
 * @param install Called when the backend becomes active.
 * @param uninstall Called when the active backend becomes inactive.
 * @param payload User data passed to the callbacks.
 *
 * @return 0 or an error code
 */
int git_backend__register(
        git_feature_t feature,
        const char *backend_name,
        git_backend__setup_cb install,
        git_backend__setup_cb uninstall,
        void *payload);

#endif
