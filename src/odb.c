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

#include "git/odb.h"

struct git_odb {
	/** Path to the "objects" directory. */
	char *path;

	/** Alternate databases to search. */
	git_odb **alternates;
};

static int open_alternates(git_odb *db)
{
	unsigned n = 0;

	db->alternates = malloc(sizeof(*db->alternates) * (n + 1));
	if (!db->alternates)
		return GIT_ERROR;

	db->alternates[n] = NULL;
	return GIT_SUCCESS;
}

int git_odb_open(git_odb **out, const char *objects_dir)
{
	git_odb *db = malloc(sizeof(*db));
	if (!db)
		return GIT_ERROR;

	db->path = strdup(objects_dir);
	if (!db->path) {
		free(db);
		return GIT_ERROR;
	}

	db->alternates = NULL;

	*out = db;
	return GIT_SUCCESS;
}

void git_odb_close(git_odb *db)
{
	if (!db)
		return;

	if (db->alternates) {
		git_odb **alt;
		for (alt = db->alternates; *alt; alt++)
			git_odb_close(*alt);
		free(db->alternates);
	}

	free(db->path);
	free(db);
}

int git_odb_read(
	git_obj *out,
	git_odb *db,
	const git_oid *id)
{
attempt:
	if (!git_odb__read_packed(out, db, id))
		return GIT_SUCCESS;
	if (!git_odb__read_loose(out, db, id))
		return GIT_SUCCESS;
	if (!db->alternates && !open_alternates(db))
		goto attempt;

	out->data = NULL;
	return GIT_ENOTFOUND;
}
