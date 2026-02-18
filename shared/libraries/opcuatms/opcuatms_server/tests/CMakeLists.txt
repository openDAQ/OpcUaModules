set(MODULE_NAME opcuatms_server)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_helpers.h
                 test_tms_device.cpp
                 test_tms_signal.cpp
                 test_tms_function_block.cpp
                 test_tms_channel.cpp
                 test_tms_input_port.cpp
                 test_tms_property_object.cpp
                 test_tms_server.cpp
                 tms_server_test.h
                 tms_server_test.cpp
)

add_executable(${TEST_APP} testapp.cpp
                           ${TEST_SOURCES}
)

if (MSVC)
    target_compile_options(${TEST_APP} PRIVATE /bigobj)
endif()

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)

target_link_libraries(${TEST_APP} PRIVATE ${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME}
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuatms_test_utils
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaclient
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_mocks
)

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)

if (COMMAND setup_target_for_coverage AND OPENDAQ_ENABLE_COVERAGE)
    setup_target_for_coverage(opcuatmsservercoverage ${TEST_APP} opcuatmsservercoverage)
endif()
