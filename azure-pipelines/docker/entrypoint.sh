#!/bin/bash -e
useradd --shell /bin/bash libgit2
chown --recursive libgit2:libgit2 /home/libgit2
exec sudo --preserve-env --set-home --user=libgit2 "$@"
