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

#include <git2.h>

BEGIN_TEST(remotes0, "remote parsing works")
	git_remote *remote;
	git_repository *repo;
	git_config *cfg;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, NULL, NULL));
	must_pass(git_remote_get(&remote, cfg, "test"));
	must_be_true(!strcmp(git_remote_name(remote), "test"));
	must_be_true(!strcmp(git_remote_url(remote), "git://github.com/libgit2/libgit2"));

	git_remote_free(remote);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(refspec0, "remote with refspec works")
	git_remote *remote;
	git_repository *repo;
	git_config *cfg;
	const git_refspec *refspec = NULL;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, NULL, NULL));
	must_pass(git_remote_get(&remote, cfg, "test"));
	refspec = git_remote_fetchspec(remote);
	must_be_true(refspec != NULL);
	must_be_true(!strcmp(git_refspec_src(refspec), "refs/heads/*"));
	must_be_true(!strcmp(git_refspec_dst(refspec), "refs/remotes/test/*"));
	git_remote_free(remote);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(refspec1, "remote fnmatch works as expected")
	git_remote *remote;
	git_repository *repo;
	git_config *cfg;
	const git_refspec *refspec = NULL;

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, NULL, NULL));
	must_pass(git_remote_get(&remote, cfg, "test"));
	refspec = git_remote_fetchspec(remote);
	must_be_true(refspec != NULL);
	must_pass(git_refspec_src_match(refspec, "refs/heads/master"));
	must_pass(git_refspec_src_match(refspec, "refs/heads/multi/level/branch"));
	git_remote_free(remote);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_TEST(refspec2, "refspec transform")
	git_remote *remote;
	git_repository *repo;
	git_config *cfg;
	const git_refspec *refspec = NULL;
	char ref[1024] = {0};

	must_pass(git_repository_open(&repo, REPOSITORY_FOLDER));
	must_pass(git_repository_config(&cfg, repo, NULL, NULL));
	must_pass(git_remote_get(&remote, cfg, "test"));
	refspec = git_remote_fetchspec(remote);
	must_be_true(refspec != NULL);
	must_pass(git_refspec_transform(ref, sizeof(ref), refspec, "refs/heads/master"));
	must_be_true(!strcmp(ref, "refs/remotes/test/master"));
	git_remote_free(remote);
	git_config_free(cfg);
	git_repository_free(repo);
END_TEST

BEGIN_SUITE(remotes)
	 ADD_TEST(remotes0)
	 ADD_TEST(refspec0)
	 ADD_TEST(refspec1)
	 ADD_TEST(refspec2)
END_SUITE
