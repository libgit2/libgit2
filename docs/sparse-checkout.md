# Sparse checkout and filtered clone

libgit2 supports cone-mode sparse checkout through `git2/sparse.h`.
Root-level files are always included. A sparse selection contains only
the explicitly requested directories and the direct files of required
intermediate directories.

For an existing repository, use `git_sparse_checkout_apply`:

```c
git_strarray directories = { paths, path_count };
git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

error = git_sparse_checkout_apply(repo, &directories, &checkout_opts);
```

The operation writes Git-compatible `core.sparseCheckout`,
`core.sparseCheckoutCone`, and `info/sparse-checkout` state. Clean paths
that become excluded are removed; locally modified excluded paths are
preserved. Use `git_sparse_checkout_disable` to restore all HEAD paths.

For a filtered sparse clone, set both clone options:

```c
git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

clone_opts.fetch_opts.filter_spec = "blob:none";
clone_opts.sparse_checkout = 1;
clone_opts.sparse_checkout_directories = directories;
```

libgit2 accepts `blob:none`, `blob:limit=<n>[kmg]`, and `tree:<depth>`
filters. The optional `k`, `m`, and `g` suffixes denote KiB, MiB, and
GiB.

When an operation reads an omitted object, the repository-owned ODB
fetches it transparently from the configured promisor remote. Applications
can also fetch known object IDs explicitly with
`git_repository_fetch_promisor`.

`git_sparse_checkout_list` returns explicit selected directories and
`git_sparse_checkout_is_enabled` reports the current enabled state.
