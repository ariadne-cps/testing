include(CTest)
add_executable(test_testing test_testing.cpp)
target_link_libraries(test_testing PRIVATE ariadne-testing testing)
add_test(NAME test_testing COMMAND $<TARGET_FILE:test_testing>)
set_tests_properties(test_testing PROPERTIES
    FAIL_REGULAR_EXPRESSION "assertion failed at")
if(WIN32)
    set_tests_properties(test_testing PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_parameterized test_parameterized.cpp)
target_link_libraries(test_parameterized PRIVATE ariadne-testing testing)
add_test(NAME test_parameterized COMMAND $<TARGET_FILE:test_parameterized>)
if(WIN32)
    set_tests_properties(test_parameterized PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_requirements test_requirements.cpp)
target_link_libraries(test_requirements PRIVATE ariadne-testing testing)
add_test(NAME test_requirements COMMAND $<TARGET_FILE:test_requirements>)
if(WIN32)
    set_tests_properties(test_requirements PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_payload test_payload.cpp)
target_link_libraries(test_payload PRIVATE ariadne-testing testing)
add_test(NAME test_payload COMMAND $<TARGET_FILE:test_payload>)
if(WIN32)
    set_tests_properties(test_payload PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_ranges test_ranges.cpp)
target_link_libraries(test_ranges PRIVATE ariadne-testing testing)
add_test(NAME test_ranges COMMAND $<TARGET_FILE:test_ranges>)
if(WIN32)
    set_tests_properties(test_ranges PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_artifacts test_artifacts.cpp)
target_link_libraries(test_artifacts PRIVATE ariadne-testing testing)
add_test(NAME test_artifacts COMMAND $<TARGET_FILE:test_artifacts>)
add_executable(test_state test_state.cpp)
target_link_libraries(test_state PRIVATE ariadne-testing testing)
add_test(NAME test_state COMMAND $<TARGET_FILE:test_state>)
add_executable(test_boundary_matrix test_boundary_matrix.cpp)
target_link_libraries(test_boundary_matrix PRIVATE ariadne-testing testing)
add_test(NAME test_boundary_matrix COMMAND $<TARGET_FILE:test_boundary_matrix>)
if(WIN32)
    set_tests_properties(test_boundary_matrix PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_evidence_enum test_evidence_enum.cpp)
target_link_libraries(test_evidence_enum PRIVATE ariadne-testing testing)
add_test(NAME test_evidence_enum COMMAND $<TARGET_FILE:test_evidence_enum>)
if(WIN32)
    set_tests_properties(test_evidence_enum PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_status test_status.cpp)
target_link_libraries(test_status PRIVATE ariadne-testing testing)
add_test(NAME test_status COMMAND $<TARGET_FILE:test_status>)
if(WIN32)
    set_tests_properties(test_status PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_alternatives test_alternatives.cpp)
target_link_libraries(test_alternatives PRIVATE ariadne-testing testing)
add_test(NAME test_alternatives COMMAND $<TARGET_FILE:test_alternatives>)
if(WIN32)
    set_tests_properties(test_alternatives PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
add_executable(test_progress test_progress.cpp)
target_link_libraries(test_progress PRIVATE ariadne-testing testing)
add_test(NAME test_progress COMMAND $<TARGET_FILE:test_progress>)
if(WIN32)
    set_tests_properties(test_progress PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()

# A process-level watchdog is essential for deadlocks that cannot be
# cancelled by a bounded in-process wait. Consumers own their own limits.
set_tests_properties(
    test_testing test_parameterized test_requirements test_payload
    test_ranges test_artifacts test_state test_boundary_matrix
    test_evidence_enum test_status test_alternatives test_progress
    PROPERTIES TIMEOUT 90
)
add_executable(test_check_macros test_check_macros.cpp)
target_link_libraries(test_check_macros PRIVATE ariadne-testing testing)
add_test(NAME test_check_macros COMMAND $<TARGET_FILE:test_check_macros>)
if(WIN32)
    set_tests_properties(test_check_macros PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_check_macros PROPERTIES TIMEOUT 90)
add_executable(test_worker test_worker.cpp)
target_link_libraries(test_worker PRIVATE ariadne-testing testing)
add_test(NAME test_worker COMMAND $<TARGET_FILE:test_worker>)
if(WIN32)
    set_tests_properties(test_worker PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_worker PROPERTIES TIMEOUT 90)
add_executable(test_deadlines test_deadlines.cpp)
target_link_libraries(test_deadlines PRIVATE ariadne-testing testing)
add_test(NAME test_deadlines COMMAND $<TARGET_FILE:test_deadlines>)
if(WIN32)
    set_tests_properties(test_deadlines PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_deadlines PROPERTIES TIMEOUT 90)
add_executable(test_timing test_timing.cpp)
target_link_libraries(test_timing PRIVATE ariadne-testing testing)
add_test(NAME test_timing COMMAND $<TARGET_FILE:test_timing>)
if(WIN32)
    set_tests_properties(test_timing PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_timing PROPERTIES TIMEOUT 90)
add_executable(test_relations test_relations.cpp)
target_link_libraries(test_relations PRIVATE ariadne-testing testing)
add_test(NAME test_relations COMMAND $<TARGET_FILE:test_relations>)
if(WIN32)
    set_tests_properties(test_relations PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_relations PROPERTIES TIMEOUT 90)
add_executable(test_lifecycle test_lifecycle.cpp)
target_link_libraries(test_lifecycle PRIVATE ariadne-testing testing)
add_test(NAME test_lifecycle COMMAND $<TARGET_FILE:test_lifecycle>)
if(WIN32)
    set_tests_properties(test_lifecycle PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_lifecycle PROPERTIES TIMEOUT 90)
# D11 standalone compile-only, negative compilation and script checks.
include("${PROJECT_SOURCE_DIR}/cmake/TestingOrchestration.cmake")
ariadne_add_compile_check(NAME compile_positive
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/positive.cpp"
 LINK_LIBRARIES testing LABELS d11)
ariadne_add_compile_check(NAME compile_negative_concept
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/negative_concept.cpp"
 LINK_LIBRARIES testing EXPECT_FAILURE
 EXPECTED_DIAGNOSTIC "ARIADNE compile-time concept requirement failed" LABELS d11)
ariadne_add_compile_check(NAME compile_negative_type
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/negative_type.cpp"
 LINK_LIBRARIES testing EXPECT_FAILURE
 EXPECTED_DIAGNOSTIC "ARIADNE compile-time type identity failed" LABELS d11)
# Compile each public layer as the FIRST (and only Testing) include.
# These tests catch transitive-include dependencies and header cycles.
ariadne_add_compile_check(NAME compile_header_results
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/header_results.cpp"
 LINK_LIBRARIES testing LABELS d11)
ariadne_add_compile_check(NAME compile_header_reporters
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/header_reporters.cpp"
 LINK_LIBRARIES testing LABELS d11)
ariadne_add_compile_check(NAME compile_header_testing
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/header_testing.cpp"
 LINK_LIBRARIES testing LABELS d11)
ariadne_add_compile_check(NAME compile_header_macros
 SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/compile/header_macros.cpp"
 LINK_LIBRARIES testing LABELS d11)
ariadne_add_cmake_script_test(NAME cmake_compile_driver
 SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_compile_driver.cmake" LABELS d11)
ariadne_add_python_unittest_test(NAME python_compile_interface
 DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/python" LABELS d11)
add_executable(test_oracle test_oracle.cpp)
target_link_libraries(test_oracle PRIVATE ariadne-testing testing)
add_test(NAME test_oracle COMMAND $<TARGET_FILE:test_oracle>)
if(WIN32)
    set_tests_properties(test_oracle PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_oracle PROPERTIES TIMEOUT 90)
add_executable(test_oracle_replay test_oracle_replay.cpp)
target_link_libraries(test_oracle_replay PRIVATE ariadne-testing testing)
add_test(NAME test_oracle_replay COMMAND $<TARGET_FILE:test_oracle_replay>)
if(WIN32)
    set_tests_properties(test_oracle_replay PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_oracle_replay PROPERTIES TIMEOUT 90)
add_executable(test_reporters test_reporters.cpp)
target_link_libraries(test_reporters PRIVATE ariadne-testing testing)
add_test(NAME test_reporters COMMAND $<TARGET_FILE:test_reporters>)
if(WIN32)
    set_tests_properties(test_reporters PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_reporters PROPERTIES TIMEOUT 90)
add_executable(test_selection test_selection.cpp)
target_link_libraries(test_selection PRIVATE ariadne-testing testing)
add_test(NAME test_selection COMMAND $<TARGET_FILE:test_selection>)
if(WIN32)
    set_tests_properties(test_selection PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:ariadne-testing>")
endif()
set_tests_properties(test_selection PROPERTIES TIMEOUT 90)
add_custom_target(tests)
add_dependencies(tests test_testing test_parameterized test_requirements test_payload test_ranges test_artifacts test_state test_boundary_matrix test_evidence_enum test_status test_alternatives test_progress test_check_macros test_worker test_deadlines test_timing test_relations test_lifecycle test_oracle test_oracle_replay test_reporters test_selection)
