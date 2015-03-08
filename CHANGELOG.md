v0.22 + 1
------

### Changes or improvements

* Updated binary identification in CRLF filtering to avoid false positives in
  UTF-8 files.

* Rename and copy detection is enabled for small files.

* Checkout can now handle an initial checkout of a repository, making
  `GIT_CHECKOUT_SAFE_CREATE` unnecessary for users of clone.

* The signature parameter in the ref-modifying functions has been
  removed. Use `git_repository_set_ident()` and
  `git_repository_ident()` to override the signature to be used.

### API additions

* Parsing and retrieving a configuration value as a path is exposed
  via `git_config_parse_path()` and `git_config_get_path()`
  respectively.

* `git_repository_set_ident()` and `git_repository_ident()` serve to
  set and query which identity will be used when writing to the
  reflog.

* `git_config_entry_free()` frees a config entry.

* `git_config_get_string_buf()` provides a way to safely retrieve a
  string from a non-snapshot configuration.

* Reference renaming now uses the right id for the old value.

### API removals

### Breaking API changes

* `GIT_CHECKOUT_SAFE_CREATE` has been removed.  Most users will generally
  be able to switch to `GIT_CHECKOUT_SAFE`, but if you require missing
  file handling during checkout, you may now use `GIT_CHECKOUT_SAFE |
  GIT_CHECKOUT_RECREATE_MISSING`.

* The `git_clone_options` and `git_submodule_update_options`
  structures no longer have a `signature` field.

* The following functions have removed the signature and/or log message
  parameters in favour of git-emulating ones.

    * `git_branch_create()`, `git_branch_move()`
    * `git_rebase_init()`, `git_rebase_abort()`
    * `git_reference_symbolic_create_matching()`,
      `git_reference_symbolic_create()`, `git_reference_create()`,
      `git_reference_create_matching()`,
      `git_reference_symbolic_set_target()`,
      `git_reference_set_target()`, `git_reference_rename()`
    * `git_remote_update_tips()`, `git_remote_fetch()`, `git_remote_push()`
    * `git_repository_set_head()`,
      `git_repository_set_head_detached()`,
      `git_repository_detach_head()`
    * `git_reset()`

* `git_config_get_entry()` now gives back a ref-counted
  `git_config_entry`. You must free it when you no longer need it.

* `git_config_get_string()` will return an error if used on a
  non-snapshot configuration, as there can be no guarantee that the
  returned pointer is valid.

v0.22
------

### Changes or improvements

* `git_signature_new()` now requires a non-empty email address.

* Use CommonCrypto libraries for SHA-1 calculation on Mac OS X.

* Disable SSL compression and SSLv2 and SSLv3 ciphers in favor of TLSv1
  in OpenSSL.

* The fetch behavior of remotes with autotag set to `GIT_REMOTE_DOWNLOAD_TAGS_ALL`
  has been changed to match git 1.9.0 and later. In this mode, libgit2 now
  fetches all tags in addition to whatever else needs to be fetched.

* `git_checkout()` now handles case-changing renames correctly on
  case-insensitive filesystems; for example renaming "readme" to "README".

* The search for libssh2 is now done via pkg-config instead of a
  custom search of a few directories.

* Add support for core.protectHFS and core.protectNTFS. Add more
  validation for filenames which we write such as references.

* The local transport now generates textual progress output like
  git-upload-pack does ("counting objects").

* `git_checkout_index()` can now check out an in-memory index that is not
  necessarily the repository's index, so you may check out an index
  that was produced by git_merge and friends while retaining the cached
  information.

* Remove the default timeout for receiving / sending data over HTTP using
  the WinHTTP transport layer.

* Add SPNEGO (Kerberos) authentication using GSSAPI on Unix systems.

* Provide built-in objects for the empty blob (e69de29) and empty
  tree (4b825dc) objects.

* The index' tree cache is now filled upon read-tree and write-tree
  and the cache is written to disk.

* LF -> CRLF filter refuses to handle mixed-EOL files

* LF -> CRLF filter now runs when * text = auto (with Git for Windows 1.9.4)

* File unlocks are atomic again via rename. Read-only files on Windows are
  made read-write if necessary.

* Share open packfiles across repositories to share descriptors and mmaps.

* Use a map for the treebuilder, making insertion O(1)

* The build system now accepts an option EMBED_SSH_PATH which when set
  tells it to include a copy of libssh2 at the given location. This is
  enabled for MSVC.

* Add support for refspecs with the asterisk in the middle of a
  pattern.

* Fetching now performs opportunistic updates. To achieve this, we
  introduce a difference between active and passive refspecs, which
  make `git_remote_download()` and `git_remote_fetch()` to take a list of
  resfpecs to be the active list, similarly to how git fetch accepts a
  list on the command-line.

* The THREADSAFE option to build libgit2 with threading support has
  been flipped to be on by default.

* The remote object has learnt to prune remote-tracking branches. If
  the remote is configured to do so, this will happen via
  `git_remote_fetch()`. You can also call `git_remote_prune()` after
  connecting or fetching to perform the prune.


### API additions

* Introduce `git_buf_text_is_binary()` and `git_buf_text_contains_nul()` for
  consumers to perform binary detection on a git_buf.

* `git_branch_upstream_remote()` has been introduced to provide the
  branch.<name>.remote configuration value.

* Introduce `git_describe_commit()` and `git_describe_workdir()` to provide
  a description of the current commit (and working tree, respectively)
  based on the nearest tag or reference

* Introduce `git_merge_bases()` and the `git_oidarray` type to expose all
  merge bases between two commits.

* Introduce `git_merge_bases_many()` to expose all merge bases between
  multiple commits.

* Introduce rebase functionality (using the merge algorithm only).
  Introduce `git_rebase_init()` to begin a new rebase session,
  `git_rebase_open()` to open an in-progress rebase session,
  `git_rebase_commit()` to commit the current rebase operation,
  `git_rebase_next()` to apply the next rebase operation,
  `git_rebase_abort()` to abort an in-progress rebase and `git_rebase_finish()`
  to complete a rebase operation.

* Introduce `git_note_author()` and `git_note_committer()` to get the author
  and committer information on a `git_note`, respectively.

* A factory function for ssh has been added which allows to change the
  path of the programs to execute for receive-pack and upload-pack on
  the server, `git_transport_ssh_with_paths()`.

* The ssh transport supports asking the remote host for accepted
  credential types as well as multiple challeges using a single
  connection. This requires to know which username you want to connect
  as, so this introduces the USERNAME credential type which the ssh
  transport will use to ask for the username.

* The `GIT_EPEEL` error code has been introduced when we cannot peel a tag
  to the requested object type; if the given object otherwise cannot be
  peeled, `GIT_EINVALIDSPEC` is returned.

* Introduce `GIT_REPOSITORY_INIT_RELATIVE_GITLINK` to use relative paths
  when writing gitlinks, as is used by git core for submodules.

* `git_remote_prune()` has been added. See above for description.


* Introduce reference transactions, which allow multiple references to
  be locked at the same time and updates be queued. This also allows
  us to safely update a reflog with arbitrary contents, as we need to
  do for stash.

### API removals

* `git_remote_supported_url()` and `git_remote_is_valid_url()` have been
  removed as they have become essentially useless with rsync-style ssh paths.

* `git_clone_into()` and `git_clone_local_into()` have been removed from the
  public API in favour of `git_clone callbacks`.

* The option to ignore certificate errors via `git_remote_cert_check()`
  is no longer present. Instead, `git_remote_callbacks` has gained a new
  entry which lets the user perform their own certificate checks.

### Breaking API changes

* `git_cherry_pick()` is now `git_cherrypick()`.

* The `git_submodule_update()` function was renamed to
  `git_submodule_update_strategy()`. `git_submodule_update()` is now used to
  provide functionalty similar to "git submodule update".

* `git_treebuilder_create()` was renamed to `git_treebuilder_new()` to better
  reflect it being a constructor rather than something which writes to
  disk.

* `git_treebuilder_new()` (was `git_treebuilder_create()`) now takes a
  repository so that it can query repository configuration.
  Subsequently, `git_treebuilder_write()` no longer takes a repository.

* `git_threads_init()` and `git_threads_shutdown()` have been renamed to
  `git_libgit2_init()` and `git_libgit2_shutdown()` to better explain what
  their purpose is, as it's grown to be more than just about threads.

* `git_libgit2_init()` and `git_libgit2_shutdown()` now return the number of
  initializations of the library, so consumers may schedule work on the
  first initialization.

* The `git_transport_register()` function no longer takes a priority and takes
  a URL scheme name (eg "http") instead of a prefix like "http://"

* `git_index_name_entrycount()` and `git_index_reuc_entrycount()` now
  return size_t instead of unsigned int.

* The `context_lines` and `interhunk_lines` fields in `git_diff`_options are
  now `uint32_t` instead of `uint16_t`. This allows to set them to `UINT_MAX`,
  in effect asking for "infinite" context e.g. to iterate over all the
  unmodified lines of a diff.

* `git_status_file()` now takes an exact path. Use `git_status_list_new()` if
  pathspec searching is needed.

* `git_note_create()` has changed the position of the notes reference
  name to match `git_note_remove()`.

* Rename `git_remote_load()` to `git_remote_lookup()` to bring it in line
  with the rest of the lookup functions.

* `git_remote_rename()` now takes the repository and the remote's
  current name. Accepting a remote indicates we want to change it,
  which we only did partially. It is much clearer if we accept a name
  and no loaded objects are changed.

* `git_remote_delete()` now accepts the repository and the remote's name
  instead of a loaded remote.

* `git_merge_head` is now `git_annotated_commit`, to better reflect its usage
  for multiple functions (including rebase)

* The `git_clone_options` struct no longer provides the `ignore_cert_errors` or
  `remote_name` members for remote customization.

  Instead, the `git_clone_options` struct has two new members, `remote_cb` and
  `remote_cb_payload`, which allow the caller to completely override the remote
  creation process. If needed, the caller can use this callback to give their
  remote a name other than the default (origin) or disable cert checking.

  The `remote_callbacks` member has been preserved for convenience, although it
  is not used when a remote creation callback is supplied.

* The `git_clone`_options struct now provides `repository_cb` and
  `repository_cb_payload` to allow the user to create a repository with
  custom options.

* The `git_push` struct to perform a push has been replaced with
  `git_remote_upload()`. The refspecs and options are passed as a
  function argument. `git_push_update_tips()` is now also
  `git_remote_update_tips()` and the callbacks are in the same struct as
  the rest.

* The `git_remote_set_transport()` function now sets a transport factory function,
  rather than a pre-existing transport instance.

* The `git_transport` structure definition has moved into the sys/transport.h
  file.

* libgit2 no longer automatically sets the OpenSSL locking
  functions. This is not something which we can know to do. A
  last-resort convenience function is provided in sys/openssl.h,
  `git_openssl_set_locking()` which can be used to set the locking.
