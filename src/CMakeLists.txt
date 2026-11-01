add_library(TESTING_SRC OBJECT testing.cpp reporters.cpp)
if(MSVC)
    foreach(_config IN ITEMS DEBUG RELWITHDEBINFO)
        string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_${_config}
            "${CMAKE_CXX_FLAGS_${_config}}")
    endforeach()
    set_property(TARGET TESTING_SRC PROPERTY MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()
