#!/bin/sh

set -x

if [ "$MBEDTLS" ]; then
  git clone https://github.com/ARMmbed/mbedtls.git ./deps/mbedtls
  cd ./deps/mbedtls
  git checkout mbedtls-2.6.1
  cmake -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=ON -DUSE_STATIC_MBEDTLS_LIBRARY=OFF .
  cmake --build .

  echo "mbedTLS built in `pwd`"
fi
