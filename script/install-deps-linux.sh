#!/bin/sh

set -x

echo "deb http://libgit2deps.edwardthomson.com trusty libgit2deps" | sudo tee -a /etc/apt/sources.list
sudo apt-key adv --keyserver pgp.mit.edu --recv 99131CD5
sudo apt-get update -qq
sudo apt-get install -y cmake curl libcurl3 libcurl3-gnutls libcurl4-gnutls-dev libssh2-1-dev openssh-client openssh-server valgrind
