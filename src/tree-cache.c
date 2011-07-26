/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "tree-cache.h"

static int read_tree_internal(git_tree_cache **out,
		const char **buffer_in, const char *buffer_end, git_tree_cache *parent)
{
	git_tree_cache *tree;
	const char *name_start, *buffer;
	int count;
	int error = GIT_SUCCESS;
	size_t name_len;

	buffer = name_start = *buffer_in;

	if ((buffer = memchr(buffer, '\0', buffer_end - buffer)) == NULL) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	if (++buffer >= buffer_end) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	name_len = strlen(name_start);
	if ((tree = git__malloc(sizeof(git_tree_cache) + name_len + 1)) == NULL)
		return GIT_ENOMEM;

	memset(tree, 0x0, sizeof(git_tree_cache));
	tree->parent = parent;

	/* NUL-terminated tree name */
	memcpy(tree->name, name_start, name_len);
	tree->name[name_len] = '\0';

	/* Blank-terminated ASCII decimal number of entries in this tree */
	if (git__strtol32(&count, buffer, &buffer, 10) < GIT_SUCCESS || count < -1) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	tree->entries = count;

	if (*buffer != ' ' || ++buffer >= buffer_end) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	 /* Number of children of the tree, newline-terminated */
	if (git__strtol32(&count, buffer, &buffer, 10) < GIT_SUCCESS ||
		count < 0) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	tree->children_count = count;

	if (*buffer != '\n' || ++buffer >= buffer_end) {
		error = GIT_EOBJCORRUPTED;
		goto cleanup;
	}

	/* The SHA1 is only there if it's not invalidated */
	if (tree->entries >= 0) {
		/* 160-bit SHA-1 for this tree and it's children */
		if (buffer + GIT_OID_RAWSZ > buffer_end) {
			error = GIT_EOBJCORRUPTED;
			goto cleanup;
		}

		git_oid_fromraw(&tree->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;
	}

	/* Parse children: */
	if (tree->children_count > 0) {
		unsigned int i;
		int err;

		tree->children = git__malloc(tree->children_count * sizeof(git_tree_cache *));
		if (tree->children == NULL)
			goto cleanup;

		for (i = 0; i < tree->children_count; ++i) {
			err = read_tree_internal(&tree->children[i], &buffer, buffer_end, tree);

			if (err < GIT_SUCCESS)
				goto cleanup;
		}
	}

	*buffer_in = buffer;
	*out = tree;
	return GIT_SUCCESS;

 cleanup:
	git_tree_cache_free(tree);
	return error;
}

int git_tree_cache_read(git_tree_cache **tree, const char *buffer, size_t buffer_size)
{
	const char *buffer_end = buffer + buffer_size;
	int error;

	error = read_tree_internal(tree, &buffer, buffer_end, NULL);

	if (buffer < buffer_end)
		return GIT_EOBJCORRUPTED;

	return error;
}

void git_tree_cache_free(git_tree_cache *tree)
{
	unsigned int i;

	if (tree == NULL)
		return;

	for (i = 0; i < tree->children_count; ++i)
		git_tree_cache_free(tree->children[i]);

	free(tree->children);
	free(tree);
}
