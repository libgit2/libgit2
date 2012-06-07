#include <git2.h>
#include <stdio.h>

int main (int argc, char** argv)
{
  git_repository *repo;
  git_index *index;
  unsigned int i, e, ecount;
  git_index_entry **entries;
  git_oid oid;

  char out[41];
  out[40] = '\0';

  git_repository_open(&repo, "/opt/libgit2-test/.git");

  git_repository_index(&index, repo);
  git_index_read(index);

  ecount = git_index_entrycount(index);
  for (i = 0; i < ecount; ++i) {
    git_index_entry *e = git_index_get(index, i);

    oid = e->oid;
    git_oid_fmt(out, &oid);

    printf("File Path: %s\n", e->path);
    printf(" Blob SHA: %s\n", out);
    printf("File Size: %d\n", (int)e->file_size);
    printf("   Device: %d\n", (int)e->dev);
    printf("    Inode: %d\n", (int)e->ino);
    printf("      UID: %d\n", (int)e->uid);
    printf("      GID: %d\n", (int)e->gid);
    printf("    ctime: %d\n", (int)e->ctime.seconds);
    printf("    mtime: %d\n", (int)e->mtime.seconds);
    printf("\n");
  }

  git_index_free(index);

  git_repository_free(repo);
}

