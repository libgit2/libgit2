#include "common.h"
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct dl_data {
	git_remote *remote;
	git_off_t *bytes;
	git_indexer_stats *stats;
	int ret;
	int finished;
};

static void *download(void *ptr)
{
	struct dl_data *data = (struct dl_data *)ptr;

	// Connect to the remote end specifying that we want to fetch
	// information from it.
	if (git_remote_connect(data->remote, GIT_DIR_FETCH) < 0) {
		data->ret = -1;
		goto exit;
	}

	// Download the packfile and index it. This function updates the
	// amount of received data and the indexer stats which lets you
	// inform the user about progress.
	if (git_remote_download(data->remote, data->bytes, data->stats) < 0) {
		data->ret = -1;
		goto exit;
	}

	data->ret = 0;

exit:
	data->finished = 1;
	pthread_exit(&data->ret);
}

int update_cb(const char *refname, const git_oid *a, const git_oid *b)
{
	const char *action;
	char a_str[GIT_OID_HEXSZ+1], b_str[GIT_OID_HEXSZ+1];

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
  git_off_t bytes = 0;
  git_indexer_stats stats;
  pthread_t worker;
  struct dl_data data;

  // Figure out whether it's a named remote or a URL
  printf("Fetching %s\n", argv[1]);
  if (git_remote_load(&remote, repo, argv[1]) < 0) {
	  if (git_remote_new(&remote, repo, NULL, argv[1], NULL) < 0)
		  return -1;
  }

  // Set up the information for the background worker thread
  data.remote = remote;
  data.bytes = &bytes;
  data.stats = &stats;
  data.ret = 0;
  data.finished = 0;
  memset(&stats, 0, sizeof(stats));

  pthread_create(&worker, NULL, download, &data);

  // Loop while the worker thread is still running. Here we show processed
  // and total objects in the pack and the amount of received
  // data. Most frontends will probably want to show a percentage and
  // the download rate.
  do {
	usleep(10000);
	printf("\rReceived %d/%d objects in %d bytes", stats.processed, stats.total, bytes);
  } while (!data.finished);
  printf("\rReceived %d/%d objects in %d bytes\n", stats.processed, stats.total, bytes);

  // Disconnect the underlying connection to prevent from idling.
  git_remote_disconnect(remote);

  // Update the references in the remote's namespace to point to the
  // right commits. This may be needed even if there was no packfile
  // to download, which can happen e.g. when the branches have been
  // changed but all the neede objects are available locally.
  if (git_remote_update_tips(remote, update_cb) < 0)
	  return -1;

  git_remote_free(remote);

  return 0;

on_error:
  git_remote_free(remote);
  return -1;
}
