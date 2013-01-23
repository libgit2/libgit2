#include "common.h"
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
# include <pthread.h>
# include <unistd.h>
#endif

struct dl_data {
	git_remote *remote;
	int ret;
	int finished;
};

static void progress_cb(const char *str, int len, void *data)
{
	data = data;
	printf("remote: %.*s", len, str);
	fflush(stdout); /* We don't have the \n to force the flush */
}

static void *download(void *ptr)
{
	struct dl_data *data = (struct dl_data *)ptr;

	// Connect to the remote end specifying that we want to fetch
	// information from it.
	if (git_remote_connect(data->remote, GIT_DIRECTION_FETCH) < 0) {
		data->ret = -1;
		goto exit;
	}

	// Download the packfile and index it. This function updates the
	// amount of received data and the indexer stats which lets you
	// inform the user about progress.
	if (git_remote_download(data->remote, NULL, NULL) < 0) {
		data->ret = -1;
		goto exit;
	}

	data->ret = 0;

exit:
	data->finished = 1;
	return &data->ret;
}

static int update_cb(const char *refname, const git_oid *a, const git_oid *b, void *data)
{
	char a_str[GIT_OID_HEXSZ+1], b_str[GIT_OID_HEXSZ+1];
	data = data;

	git_oid_fmt(b_str, b);
	b_str[GIT_OID_HEXSZ] = '\0';

	if (git_oid_iszero(a)) {
		printf("[new]     %.20s %s\n", b_str, refname);
	} else {
		git_oid_fmt(a_str, a);
		a_str[GIT_OID_HEXSZ] = '\0';
		printf("[updated] %.10s..%.10s %s\n", a_str, b_str, refname);
	}

	return 0;
}

int fetch(git_repository *repo, int argc, char **argv)
{
	git_remote *remote = NULL;
	const git_transfer_progress *stats;
	struct dl_data data;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
#ifndef _WIN32
	pthread_t worker;
#endif

	argc = argc;
	// Figure out whether it's a named remote or a URL
	printf("Fetching %s for repo %p\n", argv[1], repo);
	if (git_remote_load(&remote, repo, argv[1]) < 0) {
		if (git_remote_create_inmemory(&remote, repo, NULL, argv[1]) < 0)
			return -1;
	}

	// Set up the callbacks (only update_tips for now)
	callbacks.update_tips = &update_cb;
	callbacks.progress = &progress_cb;
	git_remote_set_callbacks(remote, &callbacks);

	// Set up the information for the background worker thread
	data.remote = remote;
	data.ret = 0;
	data.finished = 0;

	stats = git_remote_stats(remote);

#ifdef _WIN32
	download(&data);
#else
	pthread_create(&worker, NULL, download, &data);

	// Loop while the worker thread is still running. Here we show processed
	// and total objects in the pack and the amount of received
	// data. Most frontends will probably want to show a percentage and
	// the download rate.
	do {
		usleep(10000);

		if (stats->total_objects > 0)
			printf("Received %d/%d objects (%d) in %" PRIuZ " bytes\r",
			       stats->received_objects, stats->total_objects,
				   stats->indexed_objects, stats->received_bytes);
	} while (!data.finished);

	if (data.ret < 0)
		goto on_error;

	pthread_join(worker, NULL);
#endif

	printf("\rReceived %d/%d objects in %zu bytes\n",
			stats->indexed_objects, stats->total_objects, stats->received_bytes);

	// Disconnect the underlying connection to prevent from idling.
	git_remote_disconnect(remote);

	// Update the references in the remote's namespace to point to the
	// right commits. This may be needed even if there was no packfile
	// to download, which can happen e.g. when the branches have been
	// changed but all the neede objects are available locally.
	if (git_remote_update_tips(remote) < 0)
		return -1;

	git_remote_free(remote);

	return 0;

 on_error:
	git_remote_free(remote);
	return -1;
}
