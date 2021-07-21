#!/bin/sh

BUILD_DIR=build
# Setup our build dir
cd `dirname "$0"`
[ ! -d $BUILD_DIR ] && mkdir $BUILD_DIR
cd $BUILD_DIR

CMAKE="cmake"
# CMAKE="/usr/local/cmake@2.8.11/cmake"

OPENSSL_PREFIX="`brew --prefix openssl@1.1`"
# OPENSSL_PREFIX="/usr"
LIBGIT2_DEFINES="\
-DBUILD_EXAMPLES=ON \
-DBUILD_SHARED_LIBS=ON \
-DOPENSSL_ROOT_DIR=$OPENSSL_PREFIX \
-DOPENSSL_CRYPTO_LIBRARY=$OPENSSL_PREFIX/lib/libcrypto.dylib \
-DOPENSSL_SSL_LIBRARY=$OPENSSL_PREFIX/lib/libssl.dylib \
"

echo LIBGIT2_DEFINES: $LIBGIT2_DEFINES

export PKG_CONFIG_PATH="$OPENSSL_PREFIX/lib/pkgconfig"

$CMAKE .. $LIBGIT2_DEFINES $@
$CMAKE --build .
