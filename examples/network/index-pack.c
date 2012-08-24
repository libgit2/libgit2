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
  git_indexer_stream *idx;
  git_indexer_stats stats = {0, 0};
  int error, fd;
  char hash[GIT_OID_HEXSZ + 1] = {0};
  ssize_t read_bytes;
  char buf[512];

  if (argc < 2) {
	  fprintf(stderr, "I need a packfile\n");
	  return EXIT_FAILURE;
  }

  if (git_indexer_stream_new(&idx, ".git") < 0) {
	  puts("bad idx");
	  return -1;
  }

  if ((fd = open(argv[1], 0)) < 0) {
	  perror("open");
	  return -1;
  }

  do {
	  read_bytes = read(fd, buf, sizeof(buf));
	  if (read_bytes < 0)
		  break;

	  if ((error = git_indexer_stream_add(idx, buf, read_bytes, &stats)) < 0)
		  goto cleanup;

	  printf("\rIndexing %d of %d", stats.processed, stats.total);
  } while (read_bytes > 0);

  if (read_bytes < 0) {
	  error = -1;
	  perror("failed reading");
	  goto cleanup;
  }

  if ((error = git_indexer_stream_finalize(idx, &stats)) < 0)
	  goto cleanup;

  printf("\rIndexing %d of %d\n", stats.processed, stats.total);

  git_oid_fmt(hash, git_indexer_stream_hash(idx));
  puts(hash);

cleanup:
  close(fd);
  git_indexer_stream_free(idx);
  return error;
}

int index_pack_old(git_repository *repo, int argc, char **argv)
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
  if (error < 0)
    return error;

  // Index the packfile. This function can take a very long time and
  // should be run in a worker thread.
  error = git_indexer_run(indexer, &stats);
  if (error < 0)
    return error;

  // Write the information out to an index file
  error = git_indexer_write(indexer);

  // Get the packfile's hash (which should become it's filename)
  git_oid_fmt(hash, git_indexer_hash(indexer));
  puts(hash);

  git_indexer_free(indexer);

  return 0;
}
