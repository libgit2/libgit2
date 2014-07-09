v0.21 + 1
------

* File unlocks are atomic again via rename. Read-only files on Windows are
  made read-write if necessary.

* Share open packfiles across repositories to share descriptors and mmaps.

* Use a map for the treebuilder, making insertion O(1)

* LF -> CRLF filter refuses to handle mixed-EOL files

* LF -> CRLF filter now runs when * text = auto (with Git for Windows 1.9.4)

* The git_transport structure definition has moved into the sys/transport.h
  file.

* The git_transport_register function no longer takes a priority and takes
  a URL scheme name (eg "http") instead of a prefix like "http://"

* The git_remote_set_transport function now sets a transport factory function,
  rather than a pre-existing transport instance.

* A factory function for ssh has been added which allows to change the
  path of the programs to execute for receive-pack and upload-pack on
  the server, git_transport_ssh_with_paths.

* The git_clone_options struct no longer provides the ignore_cert_errors or
  remote_name members for remote customization.

  Instead, the git_clone_options struct has two new members, remote_cb and
  remote_cb_payload, which allow the caller to completely override the remote
  creation process. If needed, the caller can use this callback to give their
  remote a name other than the default (origin) or disable cert checking.

  The remote_callbacks member has been preserved for convenience, although it
  is not used when a remote creation callback is supplied.

* The git_clone_options struct now provides repository_cb and
  repository_cb_payload to allow the user to create a repository with
  custom options.

* git_clone_into and git_clone_local_into have been removed from the
  public API in favour of git_clone callbacks

* Add support for refspecs with the asterisk in the middle of a
  pattern.
