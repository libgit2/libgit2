#!/bin/sh

set -ex

brew update
brew install ninja

sudo mkdir /usr/local/lib || true
sudo chmod 0755 /usr/local/lib
sudo ln -s /Applications/Xcode.app/Contents/Developer/usr/lib/libLeaksAtExit.dylib /usr/local/lib
