/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef INCLUDE_git_tree_h__
#define INCLUDE_git_tree_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "repository.h"

/**
 * @file git2/tree.h
 * @brief Git tree parsing, loading routines
 * @defgroup git_tree Git tree parsing, loading routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Lookup a tree object from the repository.
 * The generated tree object is owned by the revision
 * repo and shall not be freed by the user.
 *
 * @param tree pointer to the looked up tree
 * @param repo the repo to use when locating the tree.
 * @param id identity of the tree to locate.
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tree_lookup(git_tree **tree, git_repository *repo, const git_oid *id)
{
	return git_repository_lookup((git_object **)tree, repo, id, GIT_OBJ_TREE);
}

/**
 * Create a new in-memory git_tree.
 *
 * The tree object must be manually filled using
 * setter methods before it can be written to its
 * repository.
 *
 * @param tree pointer to the new tree
 * @param repo The repository where the object will reside
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_tree_new(git_tree **tree, git_repository *repo)
{
	return git_repository_newobject((git_object **)tree, repo, GIT_OBJ_TREE);
}

/**
 * Get the id of a tree.
 * @param tree a previously loaded tree.
 * @return object identity for the tree.
 */
GIT_EXTERN(const git_oid *) git_tree_id(git_tree *tree);


/**
 * Get the number of entries listed in a tree
 * @param tree a previously loaded tree.
 * @return the number of entries in the tree
 */
GIT_EXTERN(size_t) git_tree_entrycount(git_tree *tree);

/**
 * Lookup a tree entry by its filename
 * @param tree a previously loaded tree.
 * @param filename the filename of the desired entry
 * @return the tree entry; NULL if not found
 */
GIT_EXTERN(git_tree_entry *) git_tree_entry_byname(git_tree *tree, const char *filename);

/**
 * Lookup a tree entry by its position in the tree
 * @param tree a previously loaded tree.
 * @param idx the position in the entry list
 * @return the tree entry; NULL if not found
 */
GIT_EXTERN(git_tree_entry *) git_tree_entry_byindex(git_tree *tree, int idx);

/**
 * Get the UNIX file attributes of a tree entry
 * @param entry a tree entry
 * @return attributes as an integer
 */
GIT_EXTERN(unsigned int) git_tree_entry_attributes(git_tree_entry *entry);

/**
 * Get the filename of a tree entry
 * @param entry a tree entry
 * @return the name of the file
 */
GIT_EXTERN(const char *) git_tree_entry_name(git_tree_entry *entry);

/**
 * Get the id of the object pointed by the entry
 * @param entry a tree entry
 * @return the oid of the object
 */
GIT_EXTERN(const git_oid *) git_tree_entry_id(git_tree_entry *entry);

/**
 * Convert a tree entry to the git_object it points too.
 *
 * @param object pointer to the converted object
 * @param entry a tree entry
 * @return a reference to the pointed object in the repository
 */
GIT_EXTERN(int) git_tree_entry_2object(git_object **object, git_tree_entry *entry);

/**
 * Add a new entry to a tree and return the new entry.
 *
 * This will mark the tree as modified; the new entry will
 * be written back to disk on the next git_object_write()
 *
 * @param entry_out Pointer to the entry that just got
 *	created. May be NULL if you are not interested on
 *	getting the new entry
 * @param tree Tree object to store the entry
 * @iparam id OID for the tree entry
 * @param filename Filename for the tree entry
 * @param attributes UNIX file attributes for the entry
 * @return 0 on success; otherwise error code
 */
GIT_EXTERN(int) git_tree_add_entry(git_tree_entry **entry_out, git_tree *tree, const git_oid *id, const char *filename, int attributes);

/**
 * Remove an entry by its index.
 *
 * Index must be >= 0 and < than git_tree_entrycount().
 *
 * This will mark the tree as modified; the modified entry will
 * be written back to disk on the next git_object_write()
 *
 * @param tree Tree where to remove the entry
 * @param idx index of the entry
 * @return 0 on successful removal; GIT_ENOTFOUND if the entry wasn't found
 */
GIT_EXTERN(int) git_tree_remove_entry_byindex(git_tree *tree, int idx);

/**
 * Remove an entry by its filename.
 *
 * This will mark the tree as modified; the modified entry will
 * be written back to disk on the next git_object_write()
 *
 * @param tree Tree where to remove the entry
 * @param filename File name of the entry
 * @return 0 on successful removal; GIT_ENOTFOUND if the entry wasn't found
 */
GIT_EXTERN(int) git_tree_remove_entry_byname(git_tree *tree, const char *filename);

/**
 * Change the SHA1 id of a tree entry.
 *
 * This will mark the tree that contains the entry as modified;
 * the modified entry will be written back to disk on the next git_object_write()
 *
 * @param entry Entry object which will be modified
 * @param oid new SHA1 oid for the entry
 */
GIT_EXTERN(void) git_tree_entry_set_id(git_tree_entry *entry, const git_oid *oid);

/**
 * Change the filename of a tree entry.
 *
 * This will mark the tree that contains the entry as modified;
 * the modified entry will be written back to disk on the next git_object_write()
 *
 * @param entry Entry object which will be modified
 * @param oid new filename for the entry
 */
GIT_EXTERN(void) git_tree_entry_set_name(git_tree_entry *entry, const char *name);

/**
 * Change the attributes of a tree entry.
 *
 * This will mark the tree that contains the entry as modified;
 * the modified entry will be written back to disk on the next git_object_write()
 *
 * @param entry Entry object which will be modified
 * @param oid new attributes for the entry
 */
GIT_EXTERN(void) git_tree_entry_set_attributes(git_tree_entry *entry, int attr);

/** @} */
GIT_END_DECL
#endif
