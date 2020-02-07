#!/bin/bash -e
useradd --shell /bin/bash libgit2
chown -R $(id -u libgit2) /home/libgit2
exec gosu libgit2 "$@"
