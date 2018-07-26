#!/bin/sh

set -x

apt-get update
apt-get -y install build-essential pkg-config clang cmake openssl libssl-dev libssh2-1-dev libcurl4-gnutls-dev openssh-server

mkdir -p /var/run/sshd
