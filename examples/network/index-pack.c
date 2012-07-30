#include <git2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "common.h"

// This could be run in the main loop whilst the application waits for
// the indexing to finish in a worker thread
static int index_cb(const git_indexer_stats *stats, void *data)
{
	data = data;
	printf("\rProcessing %d of %d", stats->processed, stats->total);

	return 0;
}

int index_pack(git_repository *repo, int argc, char **argv)
{
	git_indexer_stream *idx;
	git_indexer_stats stats = {0, 0};
	int error, fd;
	char hash[GIT_OID_HEXSZ + 1] = {0};
	ssize_t read_bytes;
	char buf[512];

	repo = repo;
	if (argc < 2) {
		fprintf(stderr, "I need a packfile\n");
		return EXIT_FAILURE;
	}

	if (git_indexer_stream_new(&idx, ".") < 0) {
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

		index_cb(&stats, NULL);
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
