#!/bin/sh

git clone https://github.com/ARMmbed/mbedtls.git mbedtls
cd mbedtls
git checkout mbedtls-2.1.2
make CFLAGS='-fPIC -fpic' -j2 lib
