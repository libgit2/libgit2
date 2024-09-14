/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_version_h__
#define INCLUDE_git_version_h__

/**
 * The version string for libgit2.  This string follows semantic
 * versioning (v2) guidelines.
 */
#define LIBGIT2_VERSION        "1.8.2"

/** The major version number for this version of libgit2. */
#define LIBGIT2_VER_MAJOR      1

/** The minor version number for this version of libgit2. */
#define LIBGIT2_VER_MINOR      8

/** The revision ("teeny") version number for this version of libgit2. */
#define LIBGIT2_VER_REVISION   2

/** The Windows DLL patch number for this version of libgit2. */
#define LIBGIT2_VER_PATCH      0

/**
 * The prerelease string for this version of libgit2.  For development
 * (nightly) builds, this will be "alpha".  For prereleases, this will be
 * a prerelease name like "beta" or "rc1".  For final releases, this will
 * be `NULL`.
 */
#define LIBGIT2_VER_PRERELEASE NULL

/**
 * The library ABI soversion for this version of libgit2. This should
 * only be changed when the library has a breaking ABI change, and so
 * may trail the library's version number.
 */
#define LIBGIT2_SOVERSION      "1.8"

/* 
 * LIBGIT2_VERSION_CHECK:
 * This macro can be used to compare against a specific libgit2 version.
 * It takes the major, minor, and patch version as parameters.
 * 
 * Usage Example:
 * 
 *   #if LIBGIT2_VERSION_CHECK(1, 7, 0) >= LIBGIT2_VERSION_NUMBER
 *     // This code will only compile if libgit2 version is >= 1.7.0
 *   #endif
 */
#define LIBGIT2_VER_CHECK(major, minor, patch)    ((major<<16)|(minor<<8)|(patch))

/* Macro to get the current version as a single integer */
#define LIBGIT2_VER_NUMBER LIBGIT2_VERSION_CHECK(LIBGIT2_VER_MAJOR, LIBGIT2_VER_MINOR, LIBGIT2_VER_PATCH)

#endif
