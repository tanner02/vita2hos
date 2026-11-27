cmake_minimum_required(VERSION 3.13)

set(VITA2HOS_MAJOR 0)
set(VITA2HOS_MINOR 4)
set(VITA2HOS_PATCH 0)
execute_process(COMMAND
    git describe --dirty --always --exclude '*'
    OUTPUT_VARIABLE VITA2HOS_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(
    vita2hos
    VERSION ${VITA2HOS_MAJOR}.${VITA2HOS_MINOR}.${VITA2HOS_PATCH}
    LANGUAGES C CXX
)

# Overwrite libnx location
set(NX_ROOT ${DEVKITPRO}/libnx32)

include(Platform/NintendoSwitch)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Horizon relocates the executable at load time. This also covers static
# dependencies such as the shader recompiler, whose code is linked into it.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(VITA_HEADERS ${CMAKE_SOURCE_DIR}/external/vita-headers)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NX_ARCH_SETTINGS} ${NX_COMMON_FLAGS} -idirafter ${VITA_HEADERS}/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NX_ARCH_SETTINGS} ${NX_COMMON_FLAGS} -idirafter ${VITA_HEADERS}/include")

add_compile_options(
    -Wall
    -Wimplicit-fallthrough=3
    -Wdouble-promotion
    -fstack-usage
    -fdata-sections
    -ffunction-sections
)

add_subdirectory(external)

add_executable(vita2hos
    source/guest_tls.c
    source/thread_events.c
    source/guest_tp.c
    source/guest_tp_plan.c
    source/guest_tp_arm.c
    source/gxm_uniform.c
    source/gxm_uniform_layout.c
    source/gxm_uniform_staging.c
    source/gxm_draw.c
    source/gxm_driver_memory.c
    source/gxm_render_surface.c
    source/gxm_viewport.c
    source/gxm_vfp.c
    source/gxm_region_clip.c
    source/gxm_blend.c
    source/gxm_surface.c
    source/gxm_diagnostics.c
    source/gxm_depth_transfer.c
    source/gxm_depth_staging.c
    source/gxm_notification.c
    source/gxm_fragment_control.c
    source/gxm_shader_code.c
    source/gxm_program_registry.c
    source/gxm_program.c
    source/gxm_program_ref.c
    source/gxm_texture.c
    source/gxm_texture_layout.c
    source/gxm_texture_staging.c
    source/deko_utils.c
    source/load.c
    source/load_bind.c
    source/load_process.c
    source/load_process_run.c
    source/load_module_info.c
    source/modules/SceModulemgr.c
    source/modules/SceRtc.c
    source/modules/SceFios2Kernel.c
    source/modules/SceProcessmgr.c
    source/load_module_paths.c
    source/load_exports.c
    source/load_fixed.c
    source/load_fixed_layout.c
    source/load_image.c
    source/load_retry.c
    source/load_layout.c
    source/load_format.c
    source/load_relocate.c
    source/load_var_relocate.c
    source/log.c
    source/log_file.c
    source/main.c
    source/module.c
    source/netlog.c
    source/uam_compiler_iface_c.cpp
    source/util.c
    source/vita3k_shader_recompiler_iface_c.cpp
    source/modules/SceCtrl.c
    source/modules/ScePerf.c
    source/modules/SceDisplay.c
    source/modules/SceGxm.c
    source/modules/SceKernelThreadMgr.c
    source/modules/SceEventFlag.c
    source/modules/SceSemaphore.c
    source/modules/SceLwMutex.c
    source/modules/SceKernelTime.c
    source/modules/SceLibKernel.c
    source/modules/SceClib.c
    source/modules/SceLibc.c
    source/modules/SceLibm.c
    source/modules/SceSysmem.c
    source/modules/SceTouch.c
)

target_include_directories(vita2hos PRIVATE
    include
    ${NX_ROOT}/include/uam/mesa-imported
)

# Only the GXM import adapters use the VFP variant PCS. Internal calls and
# libnx/deko3d keep the existing base PCS; softfp allows the adapter instructions.
set_source_files_properties(source/gxm_vfp.c PROPERTIES
    COMPILE_OPTIONS "-mfpu=neon;-mfloat-abi=softfp")

target_compile_options(vita2hos PRIVATE
    -Wextra
    -Wno-unused-parameter
)

target_compile_definitions(vita2hos PRIVATE
    VITA2HOS_MAJOR="${VITA2HOS_MAJOR}"
    VITA2HOS_MINOR="${VITA2HOS_MINOR}"
    VITA2HOS_PATCH="${VITA2HOS_PATCH}"
    VITA2HOS_HASH="${VITA2HOS_HASH}"
)

target_link_libraries(vita2hos PRIVATE
    shader
    spirv-cross-core
    fmt
    uam
    miniz
    mlib
    $<$<CONFIG:Debug>:deko3dd>
    $<$<CONFIG:Release>:deko3d>
)

target_link_directories(vita2hos PRIVATE
    ${NX_ROOT}/lib
)

target_link_options(vita2hos PRIVATE
    # Select the toolchain's PIC multilibs for libc, libm and libstdc++ too.
    -fPIE
    -specs=${NX_ROOT}/switch32.specs
    -Wl,-Map,${CMAKE_PROJECT_NAME}.map
)

find_program(NX_ELF2NSO_EXE NAMES elf2nso HINTS "${DEVKITPRO}/tools/bin")
find_program(NX_NPDMTOOL_EXE NAMES npdmtool HINTS "${DEVKITPRO}/tools/bin")
find_program(NX_BUILD_PFS0_EXE NAMES build_pfs0 HINTS "${DEVKITPRO}/tools/bin")

nx_create_exefs(${CMAKE_PROJECT_NAME}
    CONFIG ${CMAKE_PROJECT_NAME}.json
)

add_custom_target(send
    COMMAND curl -T ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.nsp --user switch:switch ftp://$ENV{SWITCHIP}:5000/vita2hos/vita2hos.nsp
    DEPENDS ${CMAKE_PROJECT_NAME}_nsp
)
