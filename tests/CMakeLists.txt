include(CTest)

set(UNIT_TESTS
    test_testing
    test_parameterized
    test_requirements
    test_payload
    test_ranges
    test_artifacts
    test_state
    test_directory_exhaustion
    test_boundary_matrix
    test_evidence_enum
    test_status
    test_alternatives
    test_progress
    test_check_macros
    test_worker
    test_deadlines
    test_timing
    test_relations
    test_lifecycle
    test_oracle
    test_oracle_replay
    test_reporters
    test_selection
    test_crash_diagnostics
)

foreach(TEST IN LISTS UNIT_TESTS)
    add_executable(${TEST} ${TEST}.cpp)
    target_link_libraries(${TEST} PRIVATE ariadne-testing testing)
    add_test(NAME ${TEST} COMMAND $<TARGET_FILE:${TEST}>)
endforeach()

set_tests_properties(test_testing PROPERTIES
    FAIL_REGULAR_EXPRESSION "assertion failed at")

# This test terminates by signal; Darwin's LLVM %c mode preserves its coverage.
if(COVERAGE AND APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
    set_tests_properties(test_crash_diagnostics PROPERTIES
        ENVIRONMENT "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/coverage/profiles/crash-%p-%c.profraw")
endif()

add_custom_target(tests)
add_dependencies(tests ${UNIT_TESTS})
