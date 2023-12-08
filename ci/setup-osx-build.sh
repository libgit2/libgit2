#!/bin/sh

set -ex

brew update
brew install pkgconfig libssh2

ln -s /Applications/Xcode.app/Contents/Developer/usr/lib/libLeaksAtExit.dylib /usr/local/lib
