#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
for test in file_io util_io; do
"${CC:-cc}" -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -Itests/host/stubs -Iinclude -Iexternal/mlib -Iexternal/vita-headers/include \
    "tests/host/$test.c" -o "$build_dir/$test"
"$build_dir/$test"
done
