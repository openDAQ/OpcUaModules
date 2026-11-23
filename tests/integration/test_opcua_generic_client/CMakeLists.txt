set(TEST_APP test_opcua_generic_client)

set(TEST_SOURCES test_app.cpp
                 opcua_test_server.h
                 test_opcua_generic_client_authentication.cpp
)

add_executable(${TEST_APP} ${TEST_SOURCES})

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)

target_link_libraries(${TEST_APP} PRIVATE ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_test_utils
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::${OPENDAQ_SDK_TARGET_NAME}
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcua_generic_client_module
                                          gtest
                                          open62541
)

# The module is linked and added to the instance in-process - no module folder is scanned
target_compile_definitions(${TEST_APP} PRIVATE OPENDAQ_SKIP_DLL_IMPORT)

set_target_properties(${TEST_APP} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>)

opendaq_target_pch(${TEST_APP}
    <opendaq/opendaq.h>
    <gtest/gtest.h>
)

# test_app.cpp defines main() and must stay its own translation unit.
set_source_files_properties(test_app.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
opendaq_target_unity_tests(${TEST_APP} BATCH_SIZE 4)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)

if (MSVC)
    target_compile_options(${TEST_APP} PRIVATE /bigobj)
endif()

if (COMMAND setup_target_for_coverage AND OPENDAQ_ENABLE_COVERAGE)
    setup_target_for_coverage(${TEST_APP}coverage ${TEST_APP} ${TEST_APP}coverage)
endif()
