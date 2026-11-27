cmake_minimum_required(VERSION 3.20)
if(NOT VITASDK)
    set(VITASDK "$ENV{VITASDK}")
endif()
if(NOT EXISTS "${VITASDK}/bin/arm-vita-eabi-gcc")
    message(FATAL_ERROR "Set VITASDK to a Vita SDK installation")
endif()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER "${VITASDK}/bin/arm-vita-eabi-gcc")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(vita2hos_surface_probe C)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
configure_file(COPYING.txt DRAW-PROBE-COPYING.txt COPYONLY)
set(FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/Vita3K/tools/native-tool/src/shaders")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/draw_shaders.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/embed-shaders.py"
        "${FIXTURES}" "${CMAKE_CURRENT_BINARY_DIR}/draw_shaders.h"
    DEPENDS embed-shaders.py "${FIXTURES}/color_v.gxp" "${FIXTURES}/clear_f.gxp"
        "${FIXTURES}/texture_v.gxp" "${FIXTURES}/texture_f.gxp" VERBATIM)
foreach(variant surface_probe draw_probe visible_cube)
    add_executable(${variant} main.c)
    set_target_properties(${variant} PROPERTIES SUFFIX ".elf")
    target_compile_options(${variant} PRIVATE -Os -Wall -Wextra -Werror
        -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns
        -march=armv7-a -mfloat-abi=softfp -mfpu=neon)
    target_link_options(${variant} PRIVATE -nostdlib -Wl,-q,-e,module_start)
    target_link_libraries(${variant} PRIVATE SceGxm_stub SceLibKernel_stub SceSysmem_stub SceIofilemgr_stub gcc)
    if(NOT variant STREQUAL "surface_probe")
        target_sources(${variant} PRIVATE draw_probe.c vertex_fetch_probe.c "${CMAKE_CURRENT_BINARY_DIR}/draw_shaders.h")
        target_include_directories(${variant} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
        target_compile_definitions(${variant} PRIVATE VITA2HOS_DRAW_PROBE)
        if(variant STREQUAL "visible_cube")
            target_sources(${variant} PRIVATE cube_scene.c)
            target_compile_definitions(${variant} PRIVATE VITA2HOS_VISIBLE_CUBE)
            target_link_libraries(${variant} PRIVATE SceDisplay_stub)
            set(destination "${CMAKE_CURRENT_BINARY_DIR}/visible-cube")
        else()
            set(destination "${CMAKE_CURRENT_BINARY_DIR}/draw-probe")
        endif()
    else()
        set(destination "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    add_custom_command(OUTPUT "${destination}/executable"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${destination}"
        COMMAND "${VITASDK}/bin/vita-elf-create" -e "${CMAKE_CURRENT_SOURCE_DIR}/exports.yml"
            "$<TARGET_FILE:${variant}>" "${destination}/executable"
        DEPENDS ${variant} exports.yml VERBATIM)
    add_custom_target(${variant}_guest ALL DEPENDS "${destination}/executable")
endforeach()

set(FIXED_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/../fixed_probe")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/fixed-probe/executable"
    COMMAND "${Python3_EXECUTABLE}" "${FIXED_PROBE}/build.py" --sdk "${VITASDK}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/fixed-probe"
    DEPENDS "${FIXED_PROBE}/build.py" "${FIXED_PROBE}/main.S" "${FIXED_PROBE}/fixed.ld"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/guest_elf.py" VERBATIM)
add_custom_target(fixed_probe_guest ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/fixed-probe/executable")

set(TLS_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/../tls_probe")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/executable"
        "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/executable.tlsplan"
        "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/manifest.json"
    COMMAND "${Python3_EXECUTABLE}" "${TLS_PROBE}/build.py" --sdk "${VITASDK}"
        --output "${CMAKE_CURRENT_BINARY_DIR}/tls-probe"
    DEPENDS "${TLS_PROBE}/build.py" "${TLS_PROBE}/main.c" "${TLS_PROBE}/thread_pointer.S"
        "${TLS_PROBE}/exports.yml" "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/guest_elf.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/make-tls-plan.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../source/modules/SceLibKernel.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../source/modules/SceKernelThreadMgr.c" VERBATIM)
add_custom_target(tls_probe_guest ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/executable"
    "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/executable.tlsplan"
    "${CMAKE_CURRENT_BINARY_DIR}/tls-probe/manifest.json")
