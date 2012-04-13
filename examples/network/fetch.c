#include "common.h"
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int fetch(git_repository *repo, int argc, char **argv)
{
  git_remote *remote = NULL;
  git_off_t bytes = 0;
  git_indexer_stats stats;
  char *packname = NULL;

  // Get the remote and connect to it
  printf("Fetching %s\n", argv[1]);
  if (git_remote_load(&remote, repo, argv[1]) == GIT_ENOTFOUND) {
	  if (git_remote_new(&remote, repo, argv[1], NULL) < 0)
		  return -1;
  }

  if (git_remote_connect(remote, GIT_DIR_FETCH) < 0)
	  return -1;

  // Download the packfile and index it
  // Doing this in a background thread and printing out what bytes
  // and stats.{processed,total} say would make the UI friendlier
  if (git_remote_download(remote, &bytes, &stats) < 0) {
	  git_remote_free(remote);
	  return -1;
  }

  printf("Received %d objects in %d bytes\n", stats.total, bytes);

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
