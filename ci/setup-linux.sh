#!/bin/sh

set -e
set -x

TMPDIR=${TMPDIR:-/tmp}

if [ -z "$SKIP_APT" ]; then
	apt-get update
	apt-get -y install build-essential pkg-config clang cmake openssl libssl-dev libssh2-1-dev libcurl4-gnutls-dev openssh-server
fi

mkdir -p /var/run/sshd
