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

#include "common.h"
#include "commit.h"
#include "revwalk.h"
#include "tree.h"

void git_tree__free(git_tree *tree)
{
	free(tree);
}

const git_oid *git_tree_id(git_tree *tree)
{
	return &tree->object.id;
}

git_tree *git_tree_lookup(git_revpool *pool, const git_oid *id)
{
	git_tree *tree = NULL;

	if (pool == NULL)
		return NULL;

	tree = (git_tree *)git_revpool_table_lookup(pool->objects, id);
	if (tree != NULL)
		return tree;

	tree = git__malloc(sizeof(git_tree));

	if (tree == NULL)
		return NULL;

	memset(tree, 0x0, sizeof(git_tree));

	/* Initialize parent object */
	git_oid_cpy(&tree->object.id, id);
	tree->object.pool = pool;
	tree->object.type = GIT_OBJ_TREE;

	git_revpool_table_insert(pool->objects, (git_revpool_object *)tree);

	return tree;
}
