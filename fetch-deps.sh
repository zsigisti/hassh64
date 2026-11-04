#!/bin/sh
# Fetch the comparison hashes and SMHasher3 at the commits used for the published numbers.
set -e
mkdir -p deps && cd deps
get() { [ -d "$1" ] || git clone -q "$2" "$1"; (cd "$1" && git checkout -q "$3"); }
get rapidhash https://github.com/Nicoshev/rapidhash 1ae7842
get wyhash    https://github.com/wangyi-fudan/wyhash e4764a0
get xxHash    https://github.com/Cyan4973/xxHash 2ec67ca
get smhasher3 https://gitlab.com/fwojcik/smhasher3.git 3b61937
echo "deps ready. For SMHasher3: copy smhasher3/hassh.cpp to deps/smhasher3/hashes/,"
echo "add it to hashes/Hashsrc.cmake, then build per SMHasher3's README."
