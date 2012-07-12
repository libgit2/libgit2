/*
 * Copyright (C) 2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_import_h__
#define INCLUDE_git_import_h__

#include "common.h"
#include "types.h"
#include "oid.h"

/**
 * @file git2/import.h
 * @brief Git fast import routines
 * @defgroup git_import Git fast import routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

typedef struct git_importer git_importer;

typedef int (*git_importer_cat_blob_callback)(
	void *payload, const git_oid *oid, const void *blob, size_t length);

GIT_EXTERN(int) git_importer_create(
	git_importer **importer_p,
	git_repository *repo);

GIT_EXTERN(int) git_importer_free(
	git_importer *importer);

GIT_EXTERN(int) git_importer_blob(
	git_importer *importer);

GIT_EXTERN(int) git_importer_mark(
	git_importer *importer, size_t mark);

GIT_EXTERN(int) git_importer_data(
	git_importer *importer, const void *buffer, size_t len);

GIT_EXTERN(int) git_importer_cat_blob_from_mark(
	git_importer *importer,
	size_t mark,
	git_importer_cat_blob_callback cb,
	void *payload);

GIT_EXTERN(int) git_importer_cat_blob_from_oid(
	git_importer *importer,
	const git_oid *oid,
	git_importer_cat_blob_callback cb,
	void *payload);

/** @} */
GIT_END_DECL
#endif
