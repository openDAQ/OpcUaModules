set(MODULE_NAME opcua_simple_server)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_simple_server.cpp
)

add_executable(${TEST_APP} testapp.cpp
                           ${TEST_SOURCES}
)

if (MSVC)
    target_compile_options(${TEST_APP} PRIVATE /bigobj)
endif()

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)

target_link_libraries(${TEST_APP} PRIVATE ${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME} gtest
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_test_utils
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaclient
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_mocks
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcua_simple_server_test_utils
)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)