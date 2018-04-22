#!/bin/sh

set -x

if [ "$MBEDTLS" ]; then
  git clone --depth 10 --single-branch --branch mbedtls-2.6.1 https://github.com/ARMmbed/mbedtls.git ./deps/mbedtls
  cd ./deps/mbedtls
  # We pass -fPIC explicitely because we'll include it in libgit2.so
  CFLAGS=-fPIC cmake -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF -DUSE_STATIC_MBEDTLS_LIBRARY=ON .
  cmake --build .

  echo "mbedTLS built in `pwd`"
fi
