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
#include "git/repository.h"

void git_tree__free(git_tree *tree)
{
	free(tree);
}

const git_oid *git_tree_id(git_tree *tree)
{
	return &tree->object.id;
}

git_tree *git_tree_lookup(git_repository *repo, const git_oid *id)
{
	return (git_tree *)git_repository_lookup(repo, id, GIT_OBJ_TREE);
}

int git_tree__parse(git_tree *tree)
{
	static const char tree_header[] = {'t', 'r', 'e', 'e', ' '};

	int error = 0;
	git_obj odb_object;
	char *buffer, *buffer_end;

	error = git_odb_read(&odb_object, tree->object.repo->db, &tree->object.id);
	if (error < 0)
		return error;

	buffer = odb_object.data;
	buffer_end = odb_object.data + odb_object.len;

	if (memcmp(buffer, tree_header, 5) != 0)
		return GIT_EOBJCORRUPTED;

	buffer += 5;

	tree->byte_size = strtol(buffer, &buffer, 10);

	if (*buffer++ != 0)
		return GIT_EOBJCORRUPTED;

	while (buffer < buffer_end) {
		git_tree_entry *entry;

		entry = git__malloc(sizeof(git_tree_entry));
		entry->next = tree->entries;

		entry->attr = strtol(buffer, &buffer, 10);

		if (*buffer++ != ' ') {
			error = GIT_EOBJCORRUPTED;
			break;
		}

		entry->filename = git__strdup(buffer);

		if (entry->filename == NULL) {
			error = GIT_EOBJCORRUPTED;
			break;
		}
		buffer += strlen(entry->filename);

		git_oid_mkraw(&entry->oid, (const unsigned char *)buffer);
		buffer += GIT_OID_RAWSZ;

		tree->entries = entry;
	}

	return error;
}
