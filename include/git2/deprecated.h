/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_deprecated_h__
#define INCLUDE_git_deprecated_h__

#include "common.h"
#include "buffer.h"
#include "errors.h"
#include "index.h"
#include "object.h"
#include "refs.h"

/*
 * Users can avoid deprecated functions by defining `GIT_DEPRECATE_HARD`.
 */
#ifndef GIT_DEPRECATE_HARD

/**
 * @file git2/deprecated.h
 * @brief libgit2 deprecated functions and values
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** @name Deprecated Buffer Functions
 *
 * These functions and enumeration values are retained for backward
 * compatibility.  The newer versions of these functions should be
 * preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

/**
 * Free the memory referred to by the git_buf.  This is an alias of
 * `git_buf_dispose` and is preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Use git_buf_dispose
 * @see git_buf_dispose
 */
GIT_EXTERN(void) git_buf_free(git_buf *buffer);

/**@}*/

/** @name Deprecated Error Functions and Constants
 *
 * These functions and enumeration values are retained for backward
 * compatibility.  The newer versions of these functions and values
 * should be preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

#define GITERR_NONE GIT_ERROR_NONE
#define GITERR_NOMEMORY GIT_ERROR_NOMEMORY
#define GITERR_OS GIT_ERROR_OS
#define GITERR_INVALID GIT_ERROR_INVALID
#define GITERR_REFERENCE GIT_ERROR_REFERENCE
#define GITERR_ZLIB GIT_ERROR_ZLIB
#define GITERR_REPOSITORY GIT_ERROR_REPOSITORY
#define GITERR_CONFIG GIT_ERROR_CONFIG
#define GITERR_REGEX GIT_ERROR_REGEX
#define GITERR_ODB GIT_ERROR_ODB
#define GITERR_INDEX GIT_ERROR_INDEX
#define GITERR_OBJECT GIT_ERROR_OBJECT
#define GITERR_NET GIT_ERROR_NET
#define GITERR_TAG GIT_ERROR_TAG
#define GITERR_TREE GIT_ERROR_TREE
#define GITERR_INDEXER GIT_ERROR_INDEXER
#define GITERR_SSL GIT_ERROR_SSL
#define GITERR_SUBMODULE GIT_ERROR_SUBMODULE
#define GITERR_THREAD GIT_ERROR_THREAD
#define GITERR_STASH GIT_ERROR_STASH
#define GITERR_CHECKOUT GIT_ERROR_CHECKOUT
#define GITERR_FETCHHEAD GIT_ERROR_FETCHHEAD
#define GITERR_MERGE GIT_ERROR_MERGE
#define GITERR_SSH GIT_ERROR_SSH
#define GITERR_FILTER GIT_ERROR_FILTER
#define GITERR_REVERT GIT_ERROR_REVERT
#define GITERR_CALLBACK GIT_ERROR_CALLBACK
#define GITERR_CHERRYPICK GIT_ERROR_CHERRYPICK
#define GITERR_DESCRIBE GIT_ERROR_DESCRIBE
#define GITERR_REBASE GIT_ERROR_REBASE
#define GITERR_FILESYSTEM GIT_ERROR_FILESYSTEM
#define GITERR_PATCH GIT_ERROR_PATCH
#define GITERR_WORKTREE GIT_ERROR_WORKTREE
#define GITERR_SHA1 GIT_ERROR_SHA1

/**
 * Return the last `git_error` object that was generated for the
 * current thread.  This is an alias of `git_error_last` and is
 * preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Use git_error_last
 * @see git_error_last
 */
GIT_EXTERN(const git_error *) giterr_last(void);

/**
 * Clear the last error.  This is an alias of `git_error_last` and is
 * preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Use git_error_clear
 * @see git_error_clear
 */
GIT_EXTERN(void) giterr_clear(void);

/**
 * Sets the error message to the given string.  This is an alias of
 * `git_error_set_str` and is preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Use git_error_set_str
 * @see git_error_set_str
 */
GIT_EXTERN(void) giterr_set_str(int error_class, const char *string);

/**
 * Indicates that an out-of-memory situation occured.  This is an alias
 * of `git_error_set_oom` and is preserved for backward compatibility.
 *
 * This function is deprecated, but there is no plan to remove this
 * function at this time.
 *
 * @deprecated Use git_error_set_oom
 * @see git_error_set_oom
 */
GIT_EXTERN(void) giterr_set_oom(void);

/**@}*/

/** @name Deprecated Index Constants
 *
 * These enumeration values are retained for backward compatibility.
 * The newer versions of these values should be preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

#define GIT_IDXENTRY_NAMEMASK          GIT_INDEX_ENTRY_NAMEMASK
#define GIT_IDXENTRY_STAGEMASK         GIT_INDEX_ENTRY_STAGEMASK
#define GIT_IDXENTRY_STAGESHIFT        GIT_INDEX_ENTRY_STAGESHIFT

/* The git_indxentry_flag_t enum */
#define GIT_IDXENTRY_EXTENDED          GIT_INDEX_ENTRY_EXTENDED
#define GIT_IDXENTRY_VALID             GIT_INDEX_ENTRY_VALID

#define GIT_IDXENTRY_STAGE(E)          GIT_INDEX_ENTRY_STAGE(E)
#define GIT_IDXENTRY_STAGE_SET(E,S)    GIT_INDEX_ENTRY_STAGE_SET(E,S)

/* The git_idxentry_extended_flag_t enum */
#define GIT_IDXENTRY_INTENT_TO_ADD     GIT_INDEX_ENTRY_INTENT_TO_ADD
#define GIT_IDXENTRY_SKIP_WORKTREE     GIT_INDEX_ENTRY_SKIP_WORKTREE
#define GIT_IDXENTRY_EXTENDED_FLAGS    (GIT_INDEX_ENTRY_INTENT_TO_ADD | GIT_INDEX_ENTRY_SKIP_WORKTREE)
#define GIT_IDXENTRY_EXTENDED2         (1 << 15)
#define GIT_IDXENTRY_UPDATE            (1 << 0)
#define GIT_IDXENTRY_REMOVE            (1 << 1)
#define GIT_IDXENTRY_UPTODATE          (1 << 2)
#define GIT_IDXENTRY_ADDED             (1 << 3)
#define GIT_IDXENTRY_HASHED            (1 << 4)
#define GIT_IDXENTRY_UNHASHED          (1 << 5)
#define GIT_IDXENTRY_WT_REMOVE         (1 << 6)
#define GIT_IDXENTRY_CONFLICTED        (1 << 7)
#define GIT_IDXENTRY_UNPACKED          (1 << 8)
#define GIT_IDXENTRY_NEW_SKIP_WORKTREE (1 << 9)

/* The git_index_capability_t enum */
#define GIT_INDEXCAP_IGNORE_CASE       GIT_INDEX_CAPABILITY_IGNORE_CASE
#define GIT_INDEXCAP_NO_FILEMODE       GIT_INDEX_CAPABILITY_NO_FILEMODE
#define GIT_INDEXCAP_NO_SYMLINKS       GIT_INDEX_CAPABILITY_NO_SYMLINKS
#define GIT_INDEXCAP_FROM_OWNER        GIT_INDEX_CAPABILITY_FROM_OWNER

/**@}*/

/** @name Deprecated Object Constants
 *
 * These enumeration values are retained for backward compatibility.  The
 * newer versions of these values should be preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

#define git_otype git_object_t

#define GIT_OBJ_ANY GIT_OBJECT_ANY
#define GIT_OBJ_BAD GIT_OBJECT_INVALID
#define GIT_OBJ__EXT1 0
#define GIT_OBJ_COMMIT GIT_OBJECT_COMMIT
#define GIT_OBJ_TREE GIT_OBJECT_TREE
#define GIT_OBJ_BLOB GIT_OBJECT_BLOB
#define GIT_OBJ_TAG GIT_OBJECT_TAG
#define GIT_OBJ__EXT2 5
#define GIT_OBJ_OFS_DELTA GIT_OBJECT_OFS_DELTA
#define GIT_OBJ_REF_DELTA GIT_OBJECT_REF_DELTA

/**@}*/

/** @name Deprecated Reference Constants
 *
 * These enumeration values are retained for backward compatibility.  The
 * newer versions of these values should be preferred in all new code.
 *
 * There is no plan to remove these backward compatibility values at
 * this time.
 */
/**@{*/

 /** Basic type of any Git reference. */
#define git_ref_t git_reference_t
#define git_reference_normalize_t git_reference_format_t

#define GIT_REF_INVALID GIT_REFERENCE_INVALID
#define GIT_REF_OID GIT_REFERENCE_DIRECT
#define GIT_REF_SYMBOLIC GIT_REFERENCE_SYMBOLIC
#define GIT_REF_LISTALL GIT_REFERENCE_ALL

#define GIT_REF_FORMAT_NORMAL GIT_REFERENCE_FORMAT_NORMAL
#define GIT_REF_FORMAT_ALLOW_ONELEVEL GIT_REFERENCE_FORMAT_ALLOW_ONELEVEL
#define GIT_REF_FORMAT_REFSPEC_PATTERN GIT_REFERENCE_FORMAT_REFSPEC_PATTERN
#define GIT_REF_FORMAT_REFSPEC_SHORTHAND GIT_REFERENCE_FORMAT_REFSPEC_SHORTHAND

/**@}*/

/** @} */
GIT_END_DECL

#endif

#endif
