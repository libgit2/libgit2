/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_textconv_h__
#define INCLUDE_sys_git_textconv_h__

typedef struct git_textconv git_textconv;

/**
 * @file git2/sys/textconv.h
 * @brief Git textconv backend and plugin routines
 * @defgroup git_backend Git custom backend APIs
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Look up a textconv by name
 *
 * @param name The name of the textconv
 * @return Pointer to the textconv object or NULL if not found
 */
GIT_EXTERN(git_textconv *) git_textconv_lookup(const char *name);

#define GIT_TEXTCONV_HTTP  "http"

/**
 * Initialize callback on textconv
 *
 * Specified as `textconv.initialize`, this is an optional callback invoked
 * before a textconv is first used.  It will be called once at most.
 *
 * If non-NULL, the textconv's `initialize` callback will be invoked right
 * before the first use of the textconv, so you can defer expensive
 * initialization operations (in case libgit2 is being used in a way that
 * doesn't need the textconv).
 */
typedef int (*git_textconv_init_fn)(git_textconv *self);

/**
 * Shutdown callback on textconv
 *
 * Specified as `textconv.shutdown`, this is an optional callback invoked
 * when the textconv is unregistered or when libgit2 is shutting down.  It
 * will be called once at most and should release resources as needed.
 * This may be called even if the `initialize` callback was not made.
 *
 * Typically this function will free the `git_textconv` object itself.
 */
typedef void (*git_textconv_shutdown_fn)(git_textconv *self);

/**
 * Callback to actually perform the data textconving
 *
 * Specified as `textconv.apply`, this is the callback that actually textconvs
 * data.  If it successfully writes the output, it should return 0. Other error codes
 * will stop textconv processing and return to the caller.
 */
typedef int (*git_textconv_apply_fn)(
                                   git_textconv    *self,
                                   git_buf       *to,
                                   const git_buf *from);

/** Callback to actually perform the data textconv in a streaming manner.
 *
 * After application, out will contain a stream to which data can be written. The textconv be
 * applied to any data written, and the result written to next.
 */
typedef int (*git_textconv_stream_fn)(
                                    git_writestream **out,
                                    git_textconv *self,
                                    git_writestream *next);

/**
 * textconv structure used to register custom textconvs.
 *
 * To associate extra data with a textconv, allocate extra data and put the
 * `git_textconv` struct at the start of your data buffer, then cast the
 * `self` pointer to your larger structure when your callback is invoked.
 */
struct git_textconv {
    /** The `version` field should be set to `GIT_TEXTCONV_VERSION`. */
    unsigned int           version;
    
    /** Called when the textconv is first used for any file. */
    git_textconv_init_fn     initialize;
    
    /** Called when the textconv is removed or unregistered from the system. */
    git_textconv_shutdown_fn shutdown;
    
    /**
     * Called to actually apply the textconv to file contents.  If this
     * function returns `GIT_PASSTHROUGH` then the contents will be passed
     * through unmodified.
     */
    git_textconv_apply_fn    apply;
    
    /**
     * Called to apply the textconv in a streaming manner.  If this is not
     * specified then the system will call `apply` with the whole buffer.
     */
    git_textconv_stream_fn   stream;
};

#define GIT_TEXTCONV_VERSION 1
#define GIT_TEXTCONV_INIT {GIT_TEXTCONV_VERSION}

/**
 * Initializes a `git_textconv` with default values. Equivalent to
 * creating an instance with GIT_TEXTCONV_INIT.
 *
 * @param textconv the `git_textconv` struct to initialize.
 * @param version Version the struct; pass `GIT_TEXTCONV_VERSION`
 * @return Zero on success; -1 on failure.
 */
GIT_EXTERN(int) git_textconv_init(git_textconv *textconv, unsigned int version);

/**
 * Register a textconv under a given name with a given priority.
 *
 * As mentioned elsewhere, the initialize callback will not be invoked
 * immediately.  It is deferred until the textconv is used in some way.
 *
 * One textconv is preregistered with libgit2:
 * - GIT_TEXTCONV_HTTP
 *
 * Currently the textconv registry is not thread safe, so any registering or
 * deregistering of textconvs must be done outside of any possible usage of
 * the textconvs (i.e. during application setup or shutdown).
 *
 * @param name A name by which the textconv can be referenced.  Attempting
 *             to register with an in-use name will return GIT_EEXISTS.
 * @param textconv The textconv definition.  This pointer will be stored as is
 *             by libgit2 so it must be a durable allocation (either static
 *             or on the heap).
 * @return 0 on successful registry, error code <0 on failure
 */
GIT_EXTERN(int) git_textconv_register(
                                    const char *name, git_textconv *textconv);

/**
 * Remove the textconv with the given name
 *
 * Attempting to remove the builtin libgit2 textconvs is not permitted and
 * will return an error.
 *
 * Currently the textconv registry is not thread safe, so any registering or
 * deregistering of textconvs must be done outside of any possible usage of
 * the textconvs (i.e. during application setup or shutdown).
 *
 * @param name The name under which the textconv was registered
 * @return 0 on success, error code <0 on failure
 */
GIT_EXTERN(int) git_textconv_unregister(const char *name);

/** @} */
GIT_END_DECL
#endif

