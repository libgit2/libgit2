#!/bin/sh

set -x

if [ -n "$COVERITY" ]; then
	./script/coverity.sh
	exit $?
fi

if [ "$TRAVIS_OS_NAME" = "osx" ]; then
	export PKG_CONFIG_PATH=$(ls -d /usr/local/Cellar/{curl,zlib}/*/lib/pkgconfig | paste -s -d':' -)

	# Set up a ramdisk for us to put our test data on to speed up tests on macOS
	export CLAR_TMP="$HOME"/_clar_tmp
	mkdir -p $CLAR_TMP

	# 5*2M sectors aka ~5GB of space
	device=$(hdiutil attach -nomount ram://$((5 * 2 * 1024 * 1024)))
	newfs_hfs $device
	mount -t hfs $device $CLAR_TMP
fi

mkdir _build
cd _build
# shellcheck disable=SC2086
cmake .. -DBUILD_EXAMPLES=ON -DENABLE_WERROR=ON -DCMAKE_INSTALL_PREFIX=../_install $OPTIONS || exit $?
cmake --build . --target install || exit $?
