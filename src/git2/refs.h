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
#ifndef INCLUDE_git_refs_h__
#define INCLUDE_git_refs_h__

#include "common.h"
#include "types.h"

/**
 * @file git2/refs.h
 * @brief Git reference management routines
 * @defgroup git_reference Git reference management routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Create a new reference.
 *
 * The reference will be empty and exclusively
 * in-memory until it is filled with the setter
 * methods and written back to disk using
 * `git_reference_write`.
 *
 * @param ref_out Pointer to the newly created reference
 * @param repo Repository where that reference exists
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_reference_new(git_reference **ref_out, git_repository *repo);

/**
 * Get the OID pointed to by a reference.
 * 
 * Only available if the reference is direct (i.e. not symbolic)
 *
 * @param ref The reference
 * @return a pointer to the oid if available, NULL otherwise
 */
GIT_EXTERN(const git_oid *) git_reference_oid(git_reference *ref);

/**
 * Get full name to the reference pointed by this reference
 * 
 * Only available if the reference is symbolic
 *
 * @param ref The reference
 * @return a pointer to the name if available, NULL otherwise
 */
GIT_EXTERN(const char *) git_reference_target(git_reference *ref);

/**
 * Get the type of a reference
 *
 * Either direct (GIT_REF_OID) or symbolic (GIT_REF_SYMBOLIC)
 *
 * @param ref The reference
 * @return the type
 */
GIT_EXTERN(git_rtype) git_reference_type(git_reference *ref);

/**
 * Get the full name of a reference
 *
 * @param ref The reference
 * @return the full name for the ref
 */
GIT_EXTERN(const char *) git_reference_name(git_reference *ref);

/**
 * Resolve a symbolic reference 
 *
 * Thie method iteratively peels a symbolic reference
 * until it resolves to a direct reference to an OID.
 *
 * If a direct reference is passed as an argument,
 * that reference is returned immediately
 *
 * @param resolved_ref Pointer to the peeled reference
 * @param ref The reference
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_reference_resolve(git_reference **resolved_ref, git_reference *ref);

/**
 * Write a reference back to disk.
 *
 * The reference must have a valid name and a valid target
 * (either direct or symbolic).
 *
 * If the reference has been loaded from disk and no changes
 * have been made, no action will take place.
 *
 * The writing to disk is atomic.
 *
 * @param ref The reference
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_reference_write(git_reference *ref);

/**
 * Get the repository where a reference resides
 *
 * @param ref The reference
 * @return a pointer to the repo
 */
GIT_EXTERN(git_repository *) git_reference_owner(git_reference *ref);

/**
 * Set the name of a reference.
 *
 * This marks the reference as modified; changes
 * won't take effect until it is manually written back
 * to disk.
 *
 * @param ref The reference
 * @param name The new name for the reference
 */
GIT_EXTERN(void) git_reference_set_name(git_reference *ref, const char *name);

/**
 * Set the target reference of a reference.
 *
 * This converts the reference into a symbolic
 * reference.
 *
 * This marks the reference as modified; changes
 * won't take effect until it is manually written back
 * to disk.
 *
 * @param ref The reference
 * @param target The new target for the reference
 */
GIT_EXTERN(void) git_reference_set_target(git_reference *ref, const char *target);

/**
 * Set the OID target of a reference.
 *
 * This converts the reference into a direct
 * reference.
 *
 * This marks the reference as modified; changes
 * won't take effect until it is manually written back
 * to disk.
 *
 * @param ref The reference
 * @param target The new target OID for the reference
 */
GIT_EXTERN(void) git_reference_set_oid(git_reference *ref, const git_oid *id);

/** @} */
GIT_END_DECL
#endif
