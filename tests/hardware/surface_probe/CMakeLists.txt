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
add_executable(surface_probe main.c)
set_target_properties(surface_probe PROPERTIES SUFFIX ".elf")
target_compile_options(surface_probe PRIVATE -Os -Wall -Wextra -Werror
    -ffreestanding -fno-builtin -fno-stack-protector -fno-tree-loop-distribute-patterns
    -march=armv7-a -mfloat-abi=softfp -mfpu=neon)
target_link_options(surface_probe PRIVATE -nostdlib -Wl,-q,-e,module_start)
target_link_libraries(surface_probe PRIVATE SceGxm_stub SceLibKernel_stub SceSysmem_stub SceIofilemgr_stub gcc)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/executable"
    COMMAND "${VITASDK}/bin/vita-elf-create" -e "${CMAKE_CURRENT_SOURCE_DIR}/exports.yml"
        "$<TARGET_FILE:surface_probe>" "${CMAKE_CURRENT_BINARY_DIR}/executable"
    DEPENDS surface_probe exports.yml VERBATIM)
add_custom_target(guest ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/executable")
