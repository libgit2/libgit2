#!/bin/sh
set -e
cd `dirname "$0"`/../build
echo 'C:\MinGW\ /MinGW' > /etc/fstab
cmake -D ENABLE_TRACE=ON -D BUILD_CLAR=ON .. -G"$GENERATOR"
cmake --build . --config RelWithDebInfo
