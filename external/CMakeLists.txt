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
