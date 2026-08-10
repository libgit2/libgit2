#!/bin/sh
export MallocStackLogging=1
export MallocScribble=1
export MallocLogFile=/dev/null
# Exclude known Apple Security framework leak in CryptKit::FEEKeyInfoProvider
# which occurs during SSL/TLS handshakes and is not in libgit2's control
export CLAR_AT_EXIT="leaks -quiet -exclude=SSLHandshake \$PPID"
exec "$@"
