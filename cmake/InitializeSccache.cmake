# Optional compiler cache preflight, shared by Windows, Linux and macOS CI.
# --show-stats is NOT sufficient: it returns successfully without a daemon.
# --zero-stats establishes a connection or starts the sccache daemon.
cmake_minimum_required(VERSION 3.21)

if("$ENV{GITHUB_ENV}" STREQUAL "")
    message(FATAL_ERROR "InitializeSccache.cmake requires GITHUB_ENV")
endif()

set(_sccache "")
if(NOT "$ENV{SCCACHE_PATH}" STREQUAL "" AND EXISTS "$ENV{SCCACHE_PATH}")
    set(_sccache "$ENV{SCCACHE_PATH}")
else()
    find_program(_found_sccache NAMES sccache)
    if(_found_sccache)
        set(_sccache "${_found_sccache}")
    endif()
endif()

set(_launcher "")
if(NOT _sccache STREQUAL "")
    foreach(_attempt RANGE 1 3)
        message(STATUS "Initializing sccache server (attempt ${_attempt}/3)")
        execute_process(
            COMMAND "${_sccache}" --zero-stats
            RESULT_VARIABLE _status
            OUTPUT_VARIABLE _out
            ERROR_VARIABLE _err
            TIMEOUT 75
        )
        if("${_status}" STREQUAL "0")
            set(_launcher "${_sccache}")
            message(STATUS "sccache server ready; compiler cache enabled")
            break()
        endif()
        string(STRIP "${_err}" _err)
        message(WARNING "sccache preflight failed (${_status}): ${_err}")
        if(_attempt LESS 3)
            execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 3)
        endif()
    endforeach()
else()
    message(WARNING "sccache not installed; compiling without cache")
endif()

# GitHub propagates this value to subsequent Configure CMake steps.
# An empty CMAKE_C{XX}_COMPILER_LAUNCHER compiles directly.
file(APPEND "$ENV{GITHUB_ENV}" "SCCACHE_LAUNCHER=${_launcher}\n")
if(_launcher STREQUAL "")
    message(WARNING "sccache disabled; using the original compiler directly")
endif()
