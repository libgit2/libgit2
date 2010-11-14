#ifndef INCLUDE_git_blob_h__
#define INCLUDE_git_blob_h__

#include "common.h"
#include "oid.h"

/**
 * @file git/blob.h
 * @brief Git blob load and write routines
 * @defgroup git_blob Git blob load and write routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** In-memory representation of a blob object. */
typedef struct git_blob git_blob;

/**
 * Lookup a blob object from a repository.
 * The generated blob object is owned by the revision
 * repo and shall not be freed by the user.
 *
 * @param blob pointer to the looked up blob
 * @param repo the repo to use when locating the blob.
 * @param id identity of the blob to locate.
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_blob_lookup(git_blob **blob, git_repository *repo, const git_oid *id);

/**
 * Create a new in-memory git_blob.
 *
 * The blob object must be manually filled using
 * the 'set_rawcontent' methods before it can
 * be written back to disk.
 *
 * @param blob pointer to the new blob
 * @param repo The repository where the object will reside
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_blob_new(git_blob **blob, git_repository *repo);

/**
 * Fill a blob with the contents inside
 * the pointed file.
 *
 * @param blob pointer to the new blob
 * @param filename name of the file to read
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_blob_set_rawcontent_fromfile(git_blob *blob, const char *filename);

/**
 * Fill a blob with the contents inside
 * the pointed buffer
 *
 * @param blob pointer to the blob
 * @param buffer buffer with the contents for the blob
 * @param len size of the buffer
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_blob_set_rawcontent(git_blob *blob, const void *buffer, size_t len);

/**
 * Read the raw content of a blob.
 *
 * A copy of the raw content is stored on the buffer passed
 * to the function. If the buffer is not long enough,
 * the method will fail.
 *
 * @param blob pointer to the blob
 * @param buffer buffer to fill with contents
 * @param len size of the buffer
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_blob_rawcontent(git_blob *blob, void *buffer, size_t len);

/**
 * Get the size in bytes of the contents of a blob
 *
 * @param blob pointer to the blob
 * @return size on bytes
 */
GIT_EXTERN(int) git_blob_rawsize(git_blob *blob);

/**
 * Read a file from the working folder of a repository
 * and write it to the Object Database as a loose blob,
 * if such doesn't exist yet.
 *
 * @param written_id return the id of the written blob
 * @param repo repository where the blob will be written
 * @param path file from which the blob will be created
 */
GIT_EXTERN(int) git_blob_writefile(git_oid *written_id, git_repository *repo, const char *path);

/** @} */
GIT_END_DECL
#endif
