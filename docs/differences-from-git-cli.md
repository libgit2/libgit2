# Differences from the Git CLI

In some instances, the functionality of libgit2 deviates slightly from that of the Git CLI. This can because of technical limitations when developing a library, licensing limitations when converting functionality from the CLI to libgit2, or various other reasons.

Repository and Workdir Path Reporting
-------------------------------------

When retrieving the absolute path of a repository from the Git CLI, one could expect the output to lool like so:

```
$ git rev-parse --absolute-git-dir
=> /home/user/projects/libgit2/.git
```

When retrieving the absolute path of a repository from libgit2, one could expect the output to look like:

```
const char *repo_path = git_repository_path(repo);
printf(repo_path);
=> /home/user/projects/libgit2/.git/
```

Notice the trailing slash. While it would be nice to be able to remove the trailing slash from the `git_repository_path` return value, it is considered a breaking change to do so, and relatively high risk for the benefit.

Retrieving the absolute path to the working directory suffers from the same problem.

Git CLI:

```bash
$ git worktree list
=> /home/user/projects/libgit2
```

libgit2:

```c
const char *workdir_path = git_repository_workdir(repo);
printf(workdir_path);
=> /home/user/projects/libgit2/
```
