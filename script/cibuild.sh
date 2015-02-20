#!/bin/sh

mkdir _build
cd _build
# shellcheck disable=SC2086
cmake .. -DCMAKE_INSTALL_PREFIX=../_install $OPTIONS || exit $?
cmake --build . --target install || exit $?
