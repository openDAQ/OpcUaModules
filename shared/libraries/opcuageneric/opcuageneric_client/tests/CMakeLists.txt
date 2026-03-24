set(MODULE_NAME opcuageneric_client)
set(TEST_APP test_${MODULE_NAME})

set(TEST_SOURCES test_app.cpp
                 test_daq_test_helper.h
                 test_opcua_generic_client_device.cpp
)

set(SUPPORTS_ASAN 0)

if(CXX_COMPILER_ID EQUAL Clang OR CXX_COMPILER_ID EQUAL AppleClang OR CXX_COMPILER_ID EQUAL GNU)
    set(SUPPORTS_ASAN 1)
    set(ASAN_COMPILE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()

add_executable(${TEST_APP} ${TEST_SOURCES})

set_target_properties(${TEST_APP} PROPERTIES DEBUG_POSTFIX _debug)

if (WIN32)
    set(BCRYPT_LIB bcrypt.dll)
endif()

target_link_libraries(${TEST_APP} PRIVATE ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_test_utils gtest
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuageneric_client
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME}
                                          ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcua_generic_client_module
)

if(SUPPORTS_ASAN)
    target_link_libraries(${TEST_APP} PRIVATE asan)
    target_compile_options(${TEST_APP} PRIVATE -Wall -Werror ${CGOV_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS})
endif()

add_test(NAME ${TEST_APP}
         COMMAND $<TARGET_FILE_NAME:${TEST_APP}>
         WORKING_DIRECTORY $<TARGET_FILE_DIR:${TEST_APP}>
)

if (MSVC)
    target_compile_options(${TEST_APP} PRIVATE /wd4324  /bigobj)
elseif (MINGW AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${TEST_APP} PRIVATE -Wa,-mbig-obj)
endif()

if (COMMAND setup_target_for_coverage AND OPENDAQ_ENABLE_COVERAGE)
    setup_target_for_coverage(${MODULE_NAME}testcoverage ${TEST_APP} ${MODULE_NAME}testcoverage)
endif()
