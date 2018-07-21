#!/bin/sh

set -x

brew update
brew install zlib
brew install curl
brew install openssl
brew install libssh2

ln -s /Applications/Xcode.app/Contents/Developer/usr/lib/libLeaksAtExit.dylib /usr/local/lib
