#!/bin/sh

set -x

# If this platform doesn't support test execution, bail out now
if [ -n "$SKIP_TESTS" ];
then
	exit $?
fi

if [ -n "$VALGRIND" -a -e "$(which valgrind)" ]; then
	valgrind --leak-check=full --show-reachable=yes --error-exitcode=125 --num-callers=50 --suppressions=./libgit2_clar.supp _build/libgit2_clar $@ -ionline -xbuf::oom
elif [ -n "$LEAKS" -a -e "$(which leaks)" ]; then
	MallocStackLogging=1 MallocScribble=1 leaks -atExit -- _build/libgit2_clar -ionline
fi
