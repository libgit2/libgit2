#!/bin/bash
exec valgrind --leak-check=full --show-reachable=yes --child-silent-after-fork=yes --error-exitcode=125 --num-callers=50 --suppressions="$(dirname "${BASH_SOURCE[0]}")/valgrind.supp" "$@"
