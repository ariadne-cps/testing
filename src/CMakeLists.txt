add_library(TESTING_SRC OBJECT testing.cpp reporters.cpp)

// MSVC /Zi writes a shared compiler PDB for both translation units.
// With parallel sccache compilation /FS is not sufficient: sccache can
// discard it, causing C1041. Embed debug symbols in each object (/Z7).
// CMake 3.25+ with CMP0141 NEW supports the target property; older
// policies keep /Zi in the configuration flags, which we replace only
// in this src/ directory (not in consumers or production targets).
if(MSVC)
    if(POLICY CMP0141)
        cmake_policy(GET CMP0141 _testing_cmp0141)
    endif()
    if(_testing_cmp0141 STREQUAL "NEW")
        set_property(TARGET TESTING_SRC PROPERTY MSVC_DEBUG_INFORMATION_FORMAT
            "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Embedded>")
    else()
        foreach(_config IN ITEMS DEBUG RELWITHDEBINFO)
            string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_${_config}
                "${CMAKE_CXX_FLAGS_${_config}}")
        endforeach()
    endif()
endif()
