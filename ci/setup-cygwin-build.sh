#!/bin/sh

set -ex

echo "##############################################################################"
echo "## Downloading cygwin"
echo "##############################################################################"

BUILD_TEMP=${BUILD_TEMP:=$TEMP}
BUILD_TEMP=$(cygpath $BUILD_TEMP)

case "$ARCH" in
        amd64)
                CYGWIN_URI="https://cygwin.com/setup-x86_64.exe";
                ARCHIVE_URI="https://mirrors.kernel.org/sourceware/cygwin";;
        x86)
                CYGWIN_URI="https://cygwin.com/setup-x86.exe";
                ARCHIVE_URI="https://mirrors.kernel.org/sourceware/cygwin-archive/20221123";;
esac

if [ -z "$CYGWIN_URI" ]; then
        echo "No URL"
        exit 1
fi

mkdir -p "$BUILD_TEMP"
mkdir -p "$BUILD_TEMP/setup"
mkdir -p "$BUILD_TEMP/packages"
mkdir -p "$BUILD_TEMP/cygwin"

curl -s -L "$CYGWIN_URI" -o "$BUILD_TEMP"/setup/cygwin-"$ARCH".exe

"$BUILD_TEMP"/setup/cygwin-"$ARCH".exe -qgnO -l "$BUILD_TEMP/packages" -R "$BUILD_TEMP/cygwin" -s "$ARCHIVE_URI" -P gcc-core,make,ninja,cmake,autotools,zlib-devel,libssl-devel,libssh2-devel,libpcre2-devel,libiconv-devel,python3,git,openssh --allow-unsupported-windows

echo BUILD_WORKSPACE="/cygdrive$(cygpath "${GITHUB_WORKSPACE}")" >> $GITHUB_ENV
