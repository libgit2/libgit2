#!/bin/sh

SHOULD_OPEN_XCODE=$1

export MACOSX_DEPLOYMENT_TARGET=10.10  # Must match GitUp

rm -rf "xcode"
mkdir "xcode"
cd "xcode"
cmake -G "Xcode" ..

# We should copy a features.h file 
# from ./xcode/src/git2/sys/features.h
# to ./git2/sys/features.h

cd ../
mv "./xcode/src/git2/sys/features.h" "./include/git2/sys/features.h"

# And open Xcode if needed.
if [ -z "$SHOULD_OPEN_XCODE" ]
then
    echo "Run Xcode if you need 'xed ./xcode/libgit2.xcodeproj'"
else
    open "./xcode/libgit2.xcodeproj"
fi
