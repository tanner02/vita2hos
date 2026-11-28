# Keep the pinned dependency pristine. Compile a build-directory copy with the
# uniform shuffle fix in both production and native integration builds.
function(vita2hos_fix_uniform_shuffle repo)
    set(original "${repo}/external/Vita3K/vita3k/shader/src/spirv_recompiler.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
    file(READ "${original}" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    set(before [==[
            std::vector<spv::Id> ops_copy_1 = { builder.createLoad(dest, spv::NoPrecision), to_copy };
            std::vector<spv::Id> ops_copy_2 = { builder.createLoad(dest_friend, spv::NoPrecision), to_copy };

            for (int i = 0; i < start % 4; i++) {
                ops_copy_1.push_back(i);
                ops_copy_2.push_back(4 + (4 - start % 4) + i);
            }

            for (int i = 0; i < (4 - start % 4); i++) {
                ops_copy_1.push_back(4 + i);
                ops_copy_2.push_back((start % 4) + i);
            }

]==])
    set(after [==[
            std::vector<spv::IdImmediate> ops_copy_1 = { { true, builder.createLoad(dest, spv::NoPrecision) }, { true, to_copy } };
            std::vector<spv::IdImmediate> ops_copy_2 = { { true, builder.createLoad(dest_friend, spv::NoPrecision) }, { true, to_copy } };

            for (int i = 0; i < start % 4; i++) {
                ops_copy_1.push_back({ false, static_cast<unsigned>(i) });
                ops_copy_2.push_back({ false, static_cast<unsigned>(4 + (4 - start % 4) + i) });
            }

            for (int i = 0; i < (4 - start % 4); i++) {
                ops_copy_1.push_back({ false, static_cast<unsigned>(4 + i) });
                ops_copy_2.push_back({ false, static_cast<unsigned>((start % 4) + i) });
            }

]==])
    string(FIND "${source}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Vita3K uniform shuffle patch no longer matches; review dependency update")
    endif()
    string(REPLACE "${before}" "${after}" patched "${source}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/vita2hos-patched/spirv_recompiler.cpp")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/vita2hos-patched")
    set(previous "")
    if(EXISTS "${output}")
        file(READ "${output}" previous)
    endif()
    if(NOT "${previous}" STREQUAL "${patched}")
        file(WRITE "${output}" "${patched}")
    endif()
    get_target_property(sources shader SOURCES)
    list(FIND sources "src/spirv_recompiler.cpp" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "Vita3K shader target source layout changed; review patch integration")
    endif()
    list(REMOVE_AT sources ${index})
    list(APPEND sources "${output}")
    set_property(TARGET shader PROPERTY SOURCES "${sources}")
endfunction()
