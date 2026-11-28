#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -DONLY_MSPACES=1 -DUSE_LOCKS=0 -DHAVE_MMAP=0 -DHAVE_MORECORE=0 -DNO_MALLOC_STATS=1 \
    -c external/dlmalloc/dlmalloc.cc -o "$build_dir/mspace_backend.o"
"${CC:-cc}" -std=gnu11 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -DTEST_THREADED_MUTEX -Itests/host/stubs -Iinclude -Iexternal/vita-headers/include \
    -c source/missing_imports.c -o "$build_dir/missing_imports.o"
"${CXX:-c++}" -std=c++17 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Iinclude -c source/missing_import_stubs.cpp -o "$build_dir/missing_import_stubs.o"
"${CC:-cc}" -std=gnu11 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Iinclude -c source/vita_sha1.c -o "$build_dir/vita_sha1.o"
"${CC:-cc}" -std=gnu11 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Itests/host/stubs -Iinclude -c tests/host/crypto_adapter.c -o "$build_dir/trophy_archive.o"
for source in source/trophy_metadata.cpp external/Vita3K/external/pugixml/src/pugixml.cpp; do
    name=$(basename "$source" .cpp)
    "${CXX:-c++}" -std=c++17 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
        -DPUGIXML_NO_EXCEPTIONS -DPUGIXML_NO_XPATH -Iinclude -Iexternal/Vita3K/external/pugixml/src \
        -c "$source" -o "$build_dir/$name.o"
done
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Iinclude -Iexternal/Vita3K/external/pugixml/src tests/host/trophy_metadata.cpp "$build_dir/trophy_metadata.o" "$build_dir/pugixml.o" \
    -o "$build_dir/trophy_metadata"
"$build_dir/trophy_metadata"
"${CC:-cc}" -std=gnu11 -O2 -fsanitize=address,undefined -Iinclude \
    -c source/savedata_mount.c -o "$build_dir/savedata_mount.o"
# Compile the same prepared ATRAC9 sources used by the native target.
atrac9_source=external/Vita3K/external/LibAtrac9/C/src
python3 tools/prepare-atrac9.py --source "$atrac9_source" --output "$build_dir/atrac9"
for source in "$atrac9_source"/*.c; do
    name=$(basename "$source")
    case "$name" in utility.c|bit_reader.c|bit_allocation.c|unpack.c) source="$build_dir/atrac9/$name" ;; esac
    "${CC:-cc}" -std=c99 -O2 -fsanitize=address,undefined -fno-sanitize-recover=all \
        -I"$atrac9_source" -c "$source" -o "$build_dir/atrac9-${name%.c}.o"
done
ar rcs "$build_dir/libvita_atrac9.a" "$build_dir"/atrac9-*.o

for test in apputil_slots savedata_identity savedata_slots gxm_ring display_state vita_sha1 trophy_context trophy_archive trophy_lifecycle trophy_unlock net_lifecycle ngs audio_out audiodec ctrl fios_overlay file_paths file_metadata standard_io file_io file_io_threads file_io_positioned util_io log_file module_exports missing_imports module_info process_params power_tick gxm_uniform gxm_uniform_layout gxm_uniform_staging gxm_draw gxm_driver_memory gxm_render_surface gxm_viewport gxm_region_clip gxm_blend gxm_surface gxm_diagnostics gxm_depth_transfer gxm_depth_staging gxm_notification gxm_fragment_control gxm_shader_code gxm_program_registry gxm_program_ref gxm_program gxm_texture gxm_texture_layout gxm_texture_usage gxm_texture_mips gxm_texture_bc gxm_texture_staging libc_exports clib_exports libm_exports sysmem sysmem_threads event_flags semaphores lwmutex mutex lwcond cond rwlock mspace kernel_time rtc load_exports load_module_info load_bind load_process load_process_run launch_mode load_module_paths load_layout load_fixed load_retry load_format load_relocate guest_tls guest_tp guest_tp_plan thread_events thread_runtime module_start_threads tls_probe; do
extra_objects=
test_cflags=
if [ "$test" = trophy_unlock ]; then test_cflags="-ftrivial-auto-var-init=pattern"; fi
if [ "$test" = mspace ]; then extra_objects="$build_dir/mspace_backend.o -lstdc++"; fi
case "$test" in
audiodec) extra_objects="$build_dir/libvita_atrac9.a"; test_cflags="-no-pie -I$atrac9_source" ;;
apputil_slots|savedata_slots) extra_objects="$build_dir/vita_sha1.o" ;;
trophy_archive|vita_sha1) extra_objects="$build_dir/vita_sha1.o -lcrypto" ;;
trophy_lifecycle|trophy_context|trophy_unlock)
    extra_objects="$build_dir/vita_sha1.o $build_dir/trophy_archive.o $build_dir/trophy_metadata.o $build_dir/pugixml.o -lstdc++ -lcrypto" ;;
missing_imports|load_bind|load_fixed|load_process)
    extra_objects="$build_dir/missing_imports.o $build_dir/missing_import_stubs.o -lstdc++" ;;
esac
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g \
    -Itests/host/stubs -Iinclude -Iexternal/mlib -Iexternal/vita-headers/include \
    $test_cflags "tests/host/$test.c" $extra_objects "$build_dir/savedata_mount.o" -o "$build_dir/$test" -lm -pthread
"$build_dir/$test"
done

# Synthetic guest slots require low 32-bit addresses on the host.
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -Werror -no-pie \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Itests/host/stubs tests/host/startup_observer.c -o "$build_dir/startup_observer"
"$build_dir/startup_observer"

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra \
    -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Itests/host/stubs -Iinclude tests/host/uam_compile.cpp -o "$build_dir/uam_compile" -pthread
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
