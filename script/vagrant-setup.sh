#!/bin/sh

set -x

fgrep "https://dl.bintray.com/libgit2/ci-dependencies" /etc/apt/sources.list
if [ $? -ne 0 ]; then
	curl -sSL "https://bintray.com/user/downloadSubjectPublicKey?username=bintray" | sudo -E apt-key add -
	echo "deb https://dl.bintray.com/libgit2/ci-dependencies trusty libgit2deps" | sudo tee -a /etc/apt/sources.list > /dev/null
fi

# PKGS="git cmake curl libcurl3 libcurl3-gnutls libcurl4-gnutls-dev libssh2-1-dev openssh-client openssh-server valgrind default-jdk"
PKGS="git cmake pkg-config curl libcurl4 libcurl4-gnutls-dev libssh2-1-dev openssh-client openssh-server valgrind default-jdk"

sudo apt-get install -y $PKGS
# sudo apt-get install -y git

if [ ! -e libgit2 ]; then
	git clone --depth 10 file:///libgit2-src libgit2
fi
