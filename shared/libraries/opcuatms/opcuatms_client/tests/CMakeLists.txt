set(MODULE_NAME opcuatms_client)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_client_context.cpp
)

add_executable(${TEST_APP} test_app.cpp
                           ${TEST_SOURCES}
)

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)

if (WIN32)
    set(BCRYPT_LIB bcrypt.dll)
endif()

target_link_libraries(${TEST_APP} PRIVATE ${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME}
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_test_utils gtest
                                          ${BCRYPT_LIB}
)

# Guarded: the helper only exists when building inside the openDAQ super-build.
if (COMMAND opendaq_target_pch)
    opendaq_target_pch(${TEST_APP}
        <opendaq/opendaq.h>
        <gtest/gtest.h>
        <opcuaclient/opcuaclient.h>
        <opcuatms/converters/variant_converter.h>
    )
endif()

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)

if (COMMAND setup_target_for_coverage AND OPENDAQ_ENABLE_COVERAGE)
    setup_target_for_coverage(${MODULE_NAME}coverage ${TEST_APP} ${MODULE_NAME}coverage)
endif()
