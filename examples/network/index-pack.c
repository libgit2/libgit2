#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

// This could be run in the main loop whilst the application waits for
// the indexing to finish in a worker thread
int index_cb(const git_indexer_stats *stats, void *data)
{
  printf("\rProcessing %d of %d", stats->processed, stats->total);
}

int index_pack(git_repository *repo, int argc, char **argv)
{
  git_indexer *indexer;
  git_indexer_stats stats;
  int error;
  char hash[GIT_OID_HEXSZ + 1] = {0};

  if (argc < 2) {
    fprintf(stderr, "I need a packfile\n");
    return EXIT_FAILURE;
  }

  // Create a new indexer
  error = git_indexer_new(&indexer, argv[1]);
  if (error < GIT_SUCCESS)
    return error;

  // Index the packfile. This function can take a very long time and
  // should be run in a worker thread.
  error = git_indexer_run(indexer, &stats);
  if (error < GIT_SUCCESS)
    return error;

  // Write the information out to an index file
  error = git_indexer_write(indexer);

  // Get the packfile's hash (which should become it's filename)
  git_oid_fmt(hash, git_indexer_hash(indexer));
  puts(hash);

  git_indexer_free(indexer);

  return GIT_SUCCESS;
}
