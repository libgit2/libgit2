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
#ifndef INCLUDE_git_types_h__
#define INCLUDE_git_types_h__

/**
 * @file git2/types.h
 * @brief libgit2 base types
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/** Basic type (loose or packed) of any Git object. */
typedef enum {
	GIT_OBJ_ANY = -2,		/**< Object can be any of the following */
	GIT_OBJ_BAD = -1,       /**< Object is invalid. */
	GIT_OBJ__EXT1 = 0,      /**< Reserved for future use. */
	GIT_OBJ_COMMIT = 1,     /**< A commit object. */
	GIT_OBJ_TREE = 2,       /**< A tree (directory listing) object. */
	GIT_OBJ_BLOB = 3,       /**< A file revision object. */
	GIT_OBJ_TAG = 4,        /**< An annotated tag object. */
	GIT_OBJ__EXT2 = 5,      /**< Reserved for future use. */
	GIT_OBJ_OFS_DELTA = 6,  /**< A delta, base is given by an offset. */
	GIT_OBJ_REF_DELTA = 7,  /**< A delta, base is given by object id. */
} git_otype;

/** An open object database handle. */
typedef struct git_odb git_odb;

/** A custom backend in an ODB */
typedef struct git_odb_backend git_odb_backend;

/**
 * Representation of an existing git repository,
 * including all its object contents
 */
typedef struct git_repository git_repository;

/** Representation of a generic object in a repository */
typedef struct git_object git_object;

typedef struct git_revwalk git_revwalk;

/** Parsed representation of a tag object. */
typedef struct git_tag git_tag;

/** In-memory representation of a blob object. */
typedef struct git_blob git_blob;

/** Parsed representation of a commit object. */
typedef struct git_commit git_commit;

/** Representation of each one of the entries in a tree object. */
typedef struct git_tree_entry git_tree_entry;

/** Representation of a tree object. */
typedef struct git_tree git_tree;

/** Memory representation of an index file. */
typedef struct git_index git_index;

/** Time in a signature */
typedef struct git_time {
	time_t time; /** time in seconds from epoch */
	int offset; /** timezone offset, in minutes */
} git_time;

/** An action signature (e.g. for committers, taggers, etc) */
typedef struct git_signature {
	char *name; /** full name of the author */
	char *email; /** email of the author */
	git_time when; /** time when the action happened */
} git_signature;

/** In-memory representation of a reference. */
typedef struct git_reference git_reference;

/** In-memory representation of a reference to an object id. */
typedef struct git_reference_object_id git_reference_object_id;

/** In-memory representation of a reference which points at another reference. The target reference is embedded. */
typedef struct git_reference_symbolic git_reference_symbolic;

/** Basic type of any Git reference. */
typedef enum {
	GIT_REF_ANY = -2,		/** Reference can be any of the following */
	GIT_REF_OBJECT_ID = 0,	/** A reference which points at an object id */
	GIT_REF_SYMBOLIC = 1,	/** A reference which points at another reference */
} git_rtype;

/** @} */
GIT_END_DECL

#endif
