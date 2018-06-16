#!/bin/sh

if [ -f ".clang-format" ]; then
	find "src" "include" -name "*.[ch]" -a ! -path "src/xdiff/*" -a ! -path "src/hash/sha1dc/*" | xargs clang-format -i
elif [ -f "uncrustify.cfg" ]; then
	find "src" "include" -name "*.[ch]" -a ! -path "src/xdiff/*" -a ! -path "src/hash/sha1dc/*" | xargs uncrustify --replace --no-backup -l C -c uncrustify.cfg
else
	echo "no formatting config found"
	exit -1
fi

git diff --stat | tail -n1
