#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
for test in file_io util_io module_exports gxm_uniform gxm_texture gxm_texture_layout gxm_texture_staging libc_exports libm_exports sysmem event_flags load_layout; do
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g \
    -Itests/host/stubs -Iinclude -Iexternal/mlib -Iexternal/vita-headers/include \
    "tests/host/$test.c" -o "$build_dir/$test" -lm -pthread
"$build_dir/$test"
done

# Switch builds use non-PIC objects; pointer tables must still permit relocation.
"${CC:-cc}" -std=gnu11 -fno-pic -fno-pie -Iinclude \
    -c tests/host/module_exports.c -o "$build_dir/module_exports.o"
for section in TestLibrary_func_table exported_libraries; do
    readelf -SW "$build_dir/module_exports.o" | \
        awk -v name="$section" '$3 == name && $9 ~ /W/ { found = 1 } END { exit !found }'
done
