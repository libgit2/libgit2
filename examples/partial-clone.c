#include "common.h"

/*
 * Report completion of object transfer.
 */
static int transfer_progress(
	const git_indexer_progress *stats,
	void *payload)
{
	UNUSED(payload);

	if (stats->total_objects &&
	    stats->received_objects == stats->total_objects)
		printf("Received %u objects\n", stats->received_objects);

	return 0;
}

/*
 * Clone a repository with a blob filter and cone-mode sparse checkout.
 */
int lg2_partial_clone(git_repository *repo, int argc, char **argv)
{
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	git_repository *cloned_repository = NULL;
	git_strarray directories;
	const git_error *last_error;
	const char *url;
	const char *path;
	int error;

	UNUSED(repo);

	if (argc < 4) {
		fprintf(stderr,
			"usage: %s <url> <path> <directory>...\n",
			argv[0]);
		return -1;
	}

	url = argv[1];
	path = argv[2];
	directories.strings = argv + 3;
	directories.count = argc - 3;

	options.fetch_opts.filter_spec = "blob:none";
	options.fetch_opts.callbacks.credentials = cred_acquire_cb;
	options.fetch_opts.callbacks.transfer_progress = transfer_progress;
	options.sparse_checkout = 1;
	options.sparse_checkout_directories = directories;

	error = git_clone(&cloned_repository, url, path, &options);
	if (error < 0) {
		last_error = git_error_last();
		if (last_error)
			fprintf(stderr, "Partial clone failed: %s\n",
				last_error->message);
	}

	git_repository_free(cloned_repository);
	return error;
}
