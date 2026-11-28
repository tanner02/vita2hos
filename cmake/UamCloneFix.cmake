# Rebuild the SDK UAM IR and interval objects in the build tree. The installed SDK and
# its archive remain unchanged; the source headers are from that same SDK.
function(vita2hos_fix_uam_clone nx_root)
    if(CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "The Switch UAM archive patch requires a single-config generator such as Ninja")
    endif()
    set(imported "${nx_root}/include/uam/mesa-imported")
    set(original "${imported}/codegen/nv50_ir.cpp")
    set(archive "${nx_root}/lib/libuam.a")
    set(member "mesa-imported_codegen_nv50_ir.cpp.o")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}" "${archive}")
    file(READ "${original}" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    set(before [==[
   for (int s = 0; srcExists(s); ++s) {
      i->setSrc(s, pol.get(getSrc(s)));
      i->src(s).mod = src(s).mod;
   }
]==])
    set(after [==[
   for (int s = 0; srcExists(s); ++s) {
      i->setSrc(s, pol.get(getSrc(s)));
      i->src(s).mod = src(s).mod;
      i->src(s).indirect[0] = src(s).indirect[0];
      i->src(s).indirect[1] = src(s).indirect[1];
      i->src(s).usedAsPtr = src(s).usedAsPtr;
   }
]==])
    string(FIND "${source}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "UAM clone source changed; review SDK update before applying the metadata fix")
    endif()
    string(REPLACE "${before}" "${after}" patched "${source}")
    execute_process(COMMAND "${CMAKE_AR}" t "${archive}"
        OUTPUT_VARIABLE members RESULT_VARIABLE result OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REPLACE "\n" ";" members "${members}")
    set(all_members "${members}")
    list(FILTER members INCLUDE REGEX "^mesa-imported_codegen_nv50_ir[.]cpp[.]o$")
    list(LENGTH members count)
    if(NOT result EQUAL 0 OR NOT count EQUAL 1)
        message(FATAL_ERROR "UAM archive layout changed; expected exactly one ${member}")
    endif()
    set(util_member "mesa-imported_codegen_nv50_ir_util.cpp.o")
    list(FILTER all_members INCLUDE REGEX "^mesa-imported_codegen_nv50_ir_util[.]cpp[.]o$")
    list(LENGTH all_members util_count)
    if(NOT util_count EQUAL 1)
        message(FATAL_ERROR "UAM archive layout changed; expected exactly one ${util_member}")
    endif()
    set(directory "${CMAKE_CURRENT_BINARY_DIR}/vita2hos-patched/uam")
    file(MAKE_DIRECTORY "${directory}")
    set(output "${directory}/nv50_ir.cpp")
    # Avoid recompiling the object on unchanged configure runs.
    if(EXISTS "${output}")
        file(READ "${output}" previous)
    endif()
    if(NOT "${previous}" STREQUAL "${patched}")
        file(WRITE "${output}" "${patched}")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(util_original "${imported}/codegen/nv50_ir_util.cpp")
    set(util_output "${directory}/nv50_ir_util.cpp")
    set(util_patch "${CMAKE_SOURCE_DIR}/tools/prepare-uam-intervals.py")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${util_original}"
        "${imported}/codegen/nv50_ir_util.h" "${util_patch}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${util_patch}"
        --source "${util_original}" --output "${util_output}"
        RESULT_VARIABLE util_result ERROR_VARIABLE util_error)
    if(NOT util_result EQUAL 0)
        message(FATAL_ERROR "UAM interval preparation failed: ${util_error}")
    endif()
    add_library(vita_uam_codegen OBJECT "${output}" "${util_output}")
    target_include_directories(vita_uam_codegen PRIVATE "${imported}")
    # The SDK UAM is built without its unavailable debug-only compiler support.
    target_compile_definitions(vita_uam_codegen PRIVATE NDEBUG HAVE_POSIX_MEMALIGN _POSIX_C_SOURCE=200112L)
    set(fixed_archive "${directory}/libuam.a")
    add_custom_command(OUTPUT "${fixed_archive}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${archive}" "${fixed_archive}"
        COMMAND "${CMAKE_AR}" d "${fixed_archive}" "${member}" "${util_member}"
        COMMAND "${CMAKE_AR}" rcs "${fixed_archive}" $<TARGET_OBJECTS:vita_uam_codegen>
        DEPENDS vita_uam_codegen $<TARGET_OBJECTS:vita_uam_codegen> "${archive}"
        COMMAND_EXPAND_LISTS VERBATIM)
    add_custom_target(vita_uam_archive DEPENDS "${fixed_archive}")
    add_library(vita_uam STATIC IMPORTED)
    set_target_properties(vita_uam PROPERTIES IMPORTED_LOCATION "${fixed_archive}")
    add_dependencies(vita_uam vita_uam_archive)
    # Reuse cache entries across unrelated runtime changes, but invalidate them
    # whenever the SDK/backend patches, compiler options or wrapper change.
    get_directory_property(directory_options COMPILE_OPTIONS)
    get_target_property(codegen_options vita_uam_codegen COMPILE_OPTIONS)
    get_target_property(codegen_definitions vita_uam_codegen COMPILE_DEFINITIONS)
    set(cache_signature "opt3-cache1:${CMAKE_CXX_COMPILER_ID}:${CMAKE_CXX_COMPILER_VERSION}:${CMAKE_BUILD_TYPE}:${CMAKE_CXX_FLAGS}:${CMAKE_CXX_FLAGS_RELEASE}:${CMAKE_CXX_FLAGS_DEBUG}:${directory_options}:${codegen_options}:${codegen_definitions}")
    set(deko_library "${nx_root}/lib/libdeko3d.a")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(deko_library "${nx_root}/lib/libdeko3dd.a")
    endif()
    foreach(input "${archive}" "${original}" "${util_original}" "${util_patch}"
            "${CMAKE_SOURCE_DIR}/cmake/UamCloneFix.cmake"
            "${CMAKE_SOURCE_DIR}/source/uam_compiler_iface_c.cpp"
            "${CMAKE_SOURCE_DIR}/source/uam_shader_cache.c"
            "${CMAKE_SOURCE_DIR}/include/uam_dksh.h" "${deko_library}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
        file(SHA256 "${input}" input_hash)
        string(APPEND cache_signature ":${input_hash}")
    endforeach()
    string(SHA256 cache_revision "${cache_signature}")
    set(VITA2HOS_UAM_CACHE_REVISION "${cache_revision}" PARENT_SCOPE)
endfunction()
