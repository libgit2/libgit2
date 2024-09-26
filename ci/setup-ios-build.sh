#!/bin/sh

set -ex

brew update
brew install pkgconfig libssh2 ninja

ln -s /Applications/Xcode.app/Contents/Developer/usr/lib/libLeaksAtExit.dylib /usr/local/lib

curl -s -L https://raw.githubusercontent.com/leetal/ios-cmake/master/ios.toolchain.cmake -o ios.toolchain.cmake
