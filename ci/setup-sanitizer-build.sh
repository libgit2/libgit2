#!/bin/sh

set -ex

# Linux updated its ASLR randomization in a way that is incompatible with
# TSAN. See https://github.com/google/sanitizers/issues/1716
sudo sysctl vm.mmap_rnd_bits=28
