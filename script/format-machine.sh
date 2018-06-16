#!/bin/sh

SRCLIST="\
	include/git2/checkout.h
	include/git2/buffer.h
	include/git2/index.h
	include/git2/repository.h
	src/blob.h
	src/blob.c
	src/commit.h
	src/commit.c
	src/config_parse.h
	src/config_parse.c
	src/odb.h
	src/odb.c
	src/repository.c
	src/repository.h
"

TYPE=$1
shift

if [ $TYPE = "unformat" ]; then
	python3 ../unformat/ --root . $SRCLIST $@
elif [ $TYPE = "whatstyle" ]; then
	../whatstyle/whatstyle.py --difftool gitdiff -f uncrustify $SRCLIST $@
else
	echo "unknown machine $TYPE"
	exit -1
fi
