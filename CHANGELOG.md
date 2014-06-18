v0.21 + 1
------

* File unlocks are atomic again via rename. Read-only files on Windows are
  made read-write if necessary.

* Share open packfiles across repositories to share descriptors and mmaps.

* Use a map for the treebuilder, making insertion O(1)

* LF -> CRLF filter refuses to handle mixed-EOL files

* LF -> CRLF filter now runs when * text = auto (with Git for Windows 1.9.4)

