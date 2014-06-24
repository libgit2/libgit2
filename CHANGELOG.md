v0.21 + 1
------

* File unlocks are atomic again via rename. Read-only files on Windows are
  made read-write if necessary.

* Share open packfiles across repositories to share descriptors and mmaps.
