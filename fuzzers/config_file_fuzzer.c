/*
 * libgit2 config file parser fuzz target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#define UNUSED(x) (void)(x)

int foreach_cb(const git_config_entry *entry, void *payload)
{
	UNUSED(entry);
	UNUSED(payload);

	return 0;
}

static char path[] = "/tmp/git.XXXXXX";
static int fd = -1;

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();
	fd = mkstemp(path);
	if (fd < 0) {
		abort();
	}

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	git_config *cfg = NULL;
	int err = 0;

	if (ftruncate(fd, 0) !=0 ) {
		abort();
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		abort();
	}
	if ((size_t)write(fd, data, size) != size) {
		abort();
	}

	err = git_config_open_ondisk(&cfg, path);
	if (err == 0) {
		git_config_foreach(cfg, foreach_cb, NULL);
		git_config_free(cfg);
	}

	return 0;
}
