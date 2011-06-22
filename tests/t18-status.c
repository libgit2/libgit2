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
#include "test_lib.h"
#include "test_helpers.h"
#include "fileops.h"
#include "git2/status.h"

#define STATUS_FOLDER TEST_RESOURCES "/status/"
#define TEMP_STATUS_FOLDER TEMP_FOLDER "status"

static const char *test_blob_oid = "9daeafb9864cf43055ae93beb0afd6c7d144bfa4";

BEGIN_TEST(file0, "test retrieving OID from a file apart from the ODB")
	char current_workdir[GIT_PATH_MAX];
	char path_statusfiles[GIT_PATH_MAX];
	char temp_path[GIT_PATH_MAX];
	git_oid expected_id, actual_id;

	must_pass(p_getcwd(current_workdir, sizeof(current_workdir)));
	strcpy(path_statusfiles, current_workdir);
	git_path_join(path_statusfiles, path_statusfiles, TEMP_STATUS_FOLDER);

	must_pass(copydir_recurs(STATUS_FOLDER, path_statusfiles));
	git_path_join(temp_path, path_statusfiles, "test.txt");

	must_pass(git_futils_exists(temp_path));

	git_oid_fromstr(&expected_id, test_blob_oid);
	must_pass(git_status_hashfile(&actual_id, temp_path));

	must_be_true(git_oid_cmp(&expected_id, &actual_id) == 0);

	git_futils_rmdir_r(TEMP_STATUS_FOLDER, 1);
END_TEST

BEGIN_SUITE(status)
	ADD_TEST(file0);
END_SUITE

