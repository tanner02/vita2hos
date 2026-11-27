#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
for test in file_io file_io_threads util_io log_file module_exports gxm_uniform gxm_uniform_layout gxm_uniform_staging gxm_draw gxm_viewport gxm_region_clip gxm_blend gxm_surface gxm_notification gxm_fragment_control gxm_shader_code gxm_program_registry gxm_program_ref gxm_texture gxm_texture_layout gxm_texture_mips gxm_texture_bc gxm_texture_staging libc_exports libm_exports sysmem event_flags semaphores lwmutex kernel_time load_exports load_bind load_process load_process_run load_module_paths load_layout load_fixed load_retry load_format load_relocate guest_tls guest_tp thread_runtime; do
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g \
    -Itests/host/stubs -Iinclude -Iexternal/mlib -Iexternal/vita-headers/include \
    "tests/host/$test.c" -o "$build_dir/$test" -lm -pthread
"$build_dir/$test"
done

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Itests/host/stubs -Iinclude tests/host/uam_compile.cpp -o "$build_dir/uam_compile"
"$build_dir/uam_compile"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Itests/host/stubs -Iinclude tests/host/shader_translation.cpp source/gxm_uniform_layout.c -o "$build_dir/shader_translation"
"$build_dir/shader_translation"

# Switch builds use non-PIC objects; pointer tables must still permit relocation.
"${CC:-cc}" -std=gnu11 -fno-pic -fno-pie -Iinclude \
    -c tests/host/module_exports.c -o "$build_dir/module_exports.o"
for section in TestLibrary_func_table exported_libraries exported_variables; do
    readelf -SW "$build_dir/module_exports.o" | \
        awk -v name="$section" '{ for (i=1; i+6<=NF; ++i) if ($i == name && $(i+6) ~ /W/) found=1 } END { exit !found }'
done
