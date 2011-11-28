#include "common.h"
#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int rename_packfile(char *packname, git_indexer *idx)
{
  char path[GIT_PATH_MAX], oid[GIT_OID_HEXSZ + 1], *slash;
  int ret;

  strcpy(path, packname);
  slash = strrchr(path, '/');

  if (!slash)
	  return GIT_EINVALIDARGS;

  memset(oid, 0x0, sizeof(oid));
  // The name of the packfile is given by it's hash which you can get
  // with git_indexer_hash after the index has been written out to
  // disk. Rename the packfile to its "real" name in the same
  // directory as it was originally (libgit2 stores it in the folder
  // where the packs go, so a rename in place is the right thing to do here
  git_oid_fmt(oid, git_indexer_hash(idx));
  ret = sprintf(slash + 1, "pack-%s.pack", oid);
  if(ret < 0)
	  return GIT_EOSERR;

  printf("Renaming pack to %s\n", path);
  return rename(packname, path);
}

int fetch(git_repository *repo, int argc, char **argv)
{
  git_remote *remote = NULL;
  git_indexer *idx = NULL;
  git_indexer_stats stats;
  int error;
  char *packname = NULL;

  // Get the remote and connect to it
  printf("Fetching %s\n", argv[1]);
  error = git_remote_new(&remote, repo, argv[1], NULL);
  if (error < GIT_SUCCESS)
    return error;

  error = git_remote_connect(remote, GIT_DIR_FETCH);
  if (error < GIT_SUCCESS)
    return error;

  // Download the packfile from the server. As we don't know its hash
  // yet, it will get a temporary filename
  error = git_remote_download(&packname, remote);
  if (error < GIT_SUCCESS)
    return error;

  // No error and a NULL packname means no packfile was needed
  if (packname != NULL) {
	  printf("The packname is %s\n", packname);

	  // Create a new instance indexer
	  error = git_indexer_new(&idx, packname);
	  if (error < GIT_SUCCESS)
		  return error;

	  // This should be run in paralel, but it'd be too complicated for the example
	  error = git_indexer_run(idx, &stats);
	  if (error < GIT_SUCCESS)
		  return error;

	  printf("Received %d objects\n", stats.total);

	  // Write the index file. The index will be stored with the
	  // correct filename
	  error = git_indexer_write(idx);
	  if (error < GIT_SUCCESS)
		  return error;

	  error = rename_packfile(packname, idx);
	  if (error < GIT_SUCCESS)
		  return error;
  }

  // Update the references in the remote's namespace to point to the
  // right commits. This may be needed even if there was no packfile
  // to download, which can happen e.g. when the branches have been
  // changed but all the neede objects are available locally.
  error = git_remote_update_tips(remote);
  if (error < GIT_SUCCESS)
    return error;

  free(packname);
  git_indexer_free(idx);
  git_remote_free(remote);

  return GIT_SUCCESS;
}
