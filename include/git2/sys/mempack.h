/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_sys_git_odb_mempack_h__
#define INCLUDE_sys_git_odb_mempack_h__

#include "git2/common.h"
#include "git2/types.h"
#include "git2/oid.h"
#include "git2/odb.h"
#include "git2/buffer.h"

/**
 * @file git2/sys/mempack.h
 * @brief A custom object database backend for storing objects in-memory
 * @defgroup git_mempack A custom object database backend for storing objects in-memory
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Instantiate a new mempack backend.
 *
 * The backend must be added to an existing ODB with the highest
 * priority.
 *
 *     git_mempack_new(&mempacker);
 *     git_repository_odb(&odb, repository);
 *     git_odb_add_backend(odb, mempacker, 999);
 *
 * Once the backend has been loaded, all writes to the ODB will
 * instead be queued in memory, and can be finalized with
 * `git_mempack_dump`.
 *
 * Subsequent reads will also be served from the in-memory store
 * to ensure consistency, until the memory store is dumped.
 *
 * @param out Pointer where to store the ODB backend
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_mempack_new(git_odb_backend **out);

/**
 * Write a thin packfile with the objects in the memory store.
 *
 * A thin packfile is a packfile that does not contain its transitive closure of
 * references. This is useful for efficiently distributing additions to a
 * repository over the network, but also finds use in the efficient bulk
 * addition of objects to a repository, locally.
 *
 * This operation performs the (shallow) insert operations into the
 * `git_packbuilder`, but does not write the packfile to disk;
 * see `git_packbuilder_write_buf`.
 *
 * It also does not reset the in-memory object database; see `git_mempack_reset`.
 *
 * @param backend The mempack backend
 * @param pb The packbuilder to use to write the packfile
 * @return 0 on success or an error code
 */
GIT_EXTERN(int) git_mempack_write_thin_pack(git_odb_backend *backend, git_packbuilder *pb);

/**
 * Dump all the queued in-memory writes to a packfile.
 *
 * The contents of the packfile will be stored in the given buffer.
 * It is the caller's responsibility to ensure that the generated
 * packfile is available to the repository (e.g. by writing it
 * to disk, or doing something crazy like distributing it across
 * several copies of the repository over a network).
 *
 * Once the generated packfile is available to the repository,
 * call `git_mempack_reset` to cleanup the memory store.
 *
 * Calling `git_mempack_reset` before the packfile has been
 * written to disk will result in an inconsistent repository
 * (the objects in the memory store won't be accessible).
 *
 * @param pack Buffer where to store the raw packfile
 * @param repo The active repository where the backend is loaded
 * @param backend The mempack backend
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_mempack_dump(git_buf *pack, git_repository *repo, git_odb_backend *backend);

/**
 * Reset the memory packer by clearing all the queued objects.
 *
 * This assumes that `git_mempack_dump` has been called before to
 * store all the queued objects into a single packfile.
 *
 * Alternatively, call `reset` without a previous dump to "undo"
 * all the recently written objects, giving transaction-like
 * semantics to the Git repository.
 *
 * @param backend The mempack backend
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_mempack_reset(git_odb_backend *backend);

/**
 * Get the total number of objects in mempack
 *
 * @param backend The mempack backend
 * @return the number of objects in the mempack, -1 on error
 */
GIT_EXTERN(int) git_mempack_object_count(git_odb_backend *backend);

/** @} */
GIT_END_DECL

#endif
