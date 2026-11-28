# Use Vita3K's glslang (installed glslang doesn't contain SpvBuilder.h, etc)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(Vita3K/external/glslang)

# Shim libraries for Vita3K
add_library(vita3k_shim INTERFACE)
target_include_directories(vita3k_shim INTERFACE ${PROJECT_SOURCE_DIR}/include/vita3k)

add_library(util INTERFACE)
target_link_libraries(util INTERFACE vita3k_shim)
target_include_directories(util INTERFACE Vita3K/vita3k/util/include)

add_library(features INTERFACE)
target_link_libraries(features INTERFACE vita3k_shim)
target_include_directories(features INTERFACE Vita3K/vita3k/features/include)

# Vita3K gxm
add_subdirectory(Vita3K/vita3k/gxm)
target_link_libraries(gxm INTERFACE vita3k_shim)
target_include_directories(gxm PRIVATE
    ${PROJECT_SOURCE_DIR}/include/vita3k
)

# Vita3K shader
add_subdirectory(Vita3K/vita3k/shader)
include("${PROJECT_SOURCE_DIR}/cmake/Vita3KUniformShuffle.cmake")
vita2hos_fix_uniform_shuffle("${PROJECT_SOURCE_DIR}")
include("${PROJECT_SOURCE_DIR}/cmake/Vita3KVertexLocations.cmake")
vita2hos_fix_vertex_locations("${PROJECT_SOURCE_DIR}")
target_include_directories(shader PRIVATE
    ${NX_ROOT}/include/spirv_cross
    ${NX_ROOT}/include/glslang
)

# M*LIB
add_library(mlib INTERFACE)
target_include_directories(mlib INTERFACE mlib)
# Trophy configuration parsing reuses Vita3K's pinned MIT-licensed pugixml.
add_library(vita_trophy_xml STATIC
    ${PROJECT_SOURCE_DIR}/source/trophy_metadata.cpp
    Vita3K/external/pugixml/src/pugixml.cpp)
target_compile_definitions(vita_trophy_xml PUBLIC PUGIXML_NO_EXCEPTIONS PUGIXML_NO_XPATH)
target_include_directories(vita_trophy_xml PUBLIC ${PROJECT_SOURCE_DIR}/include
    PRIVATE Vita3K/external/pugixml/src)

# Public ATRAC9 decoder, pinned by the existing recursive Vita3K submodule.
set(ATRAC9_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Vita3K/external/LibAtrac9/C/src")
if(NOT EXISTS "${ATRAC9_SOURCE}/libatrac9.c")
    message(FATAL_ERROR "Initialize recursive submodules to build ATRAC9 support")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(ATRAC9_PREPARED "${CMAKE_CURRENT_BINARY_DIR}/atrac9-compat")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/prepare-atrac9.py"
    --source "${ATRAC9_SOURCE}" --output "${ATRAC9_PREPARED}"
    RESULT_VARIABLE ATRAC9_PREPARE_RESULT)
if(NOT ATRAC9_PREPARE_RESULT EQUAL 0)
    message(FATAL_ERROR "ATRAC9 compatibility preparation failed")
endif()
file(GLOB ATRAC9_SOURCES CONFIGURE_DEPENDS "${ATRAC9_SOURCE}/*.c")
list(REMOVE_ITEM ATRAC9_SOURCES "${ATRAC9_SOURCE}/utility.c" "${ATRAC9_SOURCE}/bit_reader.c"
    "${ATRAC9_SOURCE}/bit_allocation.c" "${ATRAC9_SOURCE}/unpack.c")
add_library(vita_atrac9 STATIC ${ATRAC9_SOURCES}
    "${ATRAC9_PREPARED}/utility.c" "${ATRAC9_PREPARED}/bit_reader.c"
    "${ATRAC9_PREPARED}/bit_allocation.c" "${ATRAC9_PREPARED}/unpack.c")
target_include_directories(vita_atrac9 PUBLIC "${ATRAC9_SOURCE}")
target_compile_features(vita_atrac9 PRIVATE c_std_99)
target_link_libraries(vita_atrac9 PUBLIC m)
