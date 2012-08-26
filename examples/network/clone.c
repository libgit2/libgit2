#include "common.h"
#include <git2.h>
#include <git2/clone.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct dl_data {
	git_progress_multistage progress;
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
	data->ret = git_clone(&repo, data->url, data->path, &data->progress, &data->opts);
	if (repo) git_repository_free(repo);
	data->finished = 1;

	pthread_exit(&data->ret);
}

void print_progress(git_progress_multistage *msp)
{
	size_t composite_percentage = 0;
	int i;

	printf("Progress: ");
	for (i=0; i < msp->count; i++) {
		git_progress r = msp->stages[i];
		size_t percentage;

		if (i != 0) printf(" / ");

		percentage = r.total == 0
			? 0
			: 100 * r.current / r.total;
		composite_percentage += percentage;

		/*printf(" %zu/%zu", r.current, r.total);*/
		if (r.total == 0)
			printf("---%%");
		else
			printf("%3zu%%", percentage);
	}
	printf("  ==> ");
	/*printf("%zu/%zu ", t_num, t_den);*/
	printf("(%3zu%%)\n", msp->count == 0 ? 0 : composite_percentage / msp->count);
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
		print_progress(&data.progress);
	} while (!data.finished);
	print_progress(&data.progress);

	return data.ret;
}

