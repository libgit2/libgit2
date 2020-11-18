#!/bin/sh -e

echo "##############################################################################"
echo "## Downloading mingw"
echo "##############################################################################"

BUILD_TEMP=${BUILD_TEMP:=$TEMP}
BUILD_TEMP=$(cygpath $BUILD_TEMP)

case "$ARCH" in
	amd64)
		MINGW_URI="https://bintray.com/libgit2/build-dependencies/download_file?file_path=mingw-w64-x86_64-8.1.0-release-win32-seh-rt_v6-rev0.zip";;
	x86)
		MINGW_URI="https://bintray.com/libgit2/build-dependencies/download_file?file_path=mingw-w64-i686-8.1.0-release-win32-sjlj-rt_v6-rev0.zip";;
esac

if [ -z "$MINGW_URI" ]; then
	echo "No URL"
	exit 1
fi

mkdir -p "$BUILD_TEMP"

curl -s -L "$MINGW_URI" -o "$BUILD_TEMP"/mingw-"$ARCH".zip
unzip -q "$BUILD_TEMP"/mingw-"$ARCH".zip -d "$BUILD_TEMP"
