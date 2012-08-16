#include "common.h"
#include <git2.h>
#include <git2/clone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct dl_data {
	git_indexer_stats fetch_stats;
	git_indexer_stats checkout_stats;
	git_checkout_opts opts;
	int ret;
	int finished;
	const char *url;
	const char *path;
};

static void *clone_thread(void *ptr)
{
	struct dl_data *data = (struct dl_data *)ptr;
	git_repository *repo = NULL;

	// Kick off the clone
	data->ret = git_clone(&repo, data->url, data->path,
								 &data->fetch_stats, &data->checkout_stats, 
								 &data->opts);
	if (repo) git_repository_free(repo);
	data->finished = 1;

	pthread_exit(&data->ret);
}

int do_clone(git_repository *repo, int argc, char **argv)
{
	struct dl_data data = {0};
	pthread_t worker;

	// Validate args
	if (argc < 3) {
		printf("USAGE: %s <url> <path>\n", argv[0]);
		return -1;
	}

	// Data for background thread
	data.url = argv[1];
	data.path = argv[2];
	data.opts.disable_filters = 1;
	printf("Cloning '%s' to '%s'\n", data.url, data.path);

	// Create the worker thread
	pthread_create(&worker, NULL, clone_thread, &data);

	// Watch for progress information
	do {
		usleep(10000);
		printf("Fetch %d/%d  –  Checkout %d/%d\n",
				 data.fetch_stats.processed, data.fetch_stats.total,
				 data.checkout_stats.processed, data.checkout_stats.total);
	} while (!data.finished);
	printf("Fetch %d/%d  –  Checkout %d/%d\n",
			 data.fetch_stats.processed, data.fetch_stats.total,
			 data.checkout_stats.processed, data.checkout_stats.total);

	return data.ret;
}

