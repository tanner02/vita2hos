#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -Itests/host/stubs -Iinclude -Iexternal/mlib -Iexternal/vita-headers/include \
    tests/host/file_io.c -o "$build_dir/file_io"
"$build_dir/file_io"
