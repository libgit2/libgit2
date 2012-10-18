#include "common.h"
#include <git2.h>
#include <git2/clone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

typedef struct progress_data {
	git_indexer_stats fetch_progress;
	float checkout_progress;
	const char *path;
} progress_data;

static void print_progress(const progress_data *pd)
{
	/*
	int network_percent = (100*pd->fetch_progress.received) / pd->fetch_progress.total;
	int index_percent = (100*pd->fetch_progress.processed) / pd->fetch_progress.total;
	int checkout_percent = (int)(100.f * pd->checkout_progress);
	printf("net %3d%%  /  idx %3d%%  /  chk %3d%%  %20s\r",
			network_percent, index_percent, checkout_percent, pd->path);
			*/
	printf("net %5d /%5d  –  idx %5d /%5d  –  chk %.04f   %20s\r",
			pd->fetch_progress.received, pd->fetch_progress.total,
			pd->fetch_progress.processed, pd->fetch_progress.total,
			pd->checkout_progress, pd->path);
}

static void fetch_progress(const git_indexer_stats *stats, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->fetch_progress = *stats;
	print_progress(pd);
}
static void checkout_progress(const char *path, float progress, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->checkout_progress = progress;
	pd->path = path;
	print_progress(pd);
}

int do_clone(git_repository *repo, int argc, char **argv)
{
	progress_data pd = {0};
	git_repository *cloned_repo = NULL;
	git_checkout_opts checkout_opts = {0};
	const char *url = argv[1];
	const char *path = argv[2];
	int error;

	// Validate args
	if (argc < 3) {
		printf ("USAGE: %s <url> <path>\n", argv[0]);
		return -1;
	}

	// Set up options
	checkout_opts.checkout_strategy = GIT_CHECKOUT_CREATE_MISSING;
	checkout_opts.progress_cb = checkout_progress;
	checkout_opts.progress_payload = &pd;

	// Do the clone
	error = git_clone(&cloned_repo, url, path, &fetch_progress, &pd, &checkout_opts);
	printf("\n");
	if (error != 0) {
		const git_error *err = giterr_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}
	else if (cloned_repo) git_repository_free(cloned_repo);
	return error;
}
