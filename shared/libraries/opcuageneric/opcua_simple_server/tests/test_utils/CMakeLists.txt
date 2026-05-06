set(MODULE_NAME opcua_simple_server_test_utils)

set(MODULE_SOURCES generic_opcua_test_helper.h
                   generic_opcua_test_helper.cpp
                   test_helpers.h
)

add_library(${MODULE_NAME} STATIC ${MODULE_SOURCES})
add_library(${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME} ALIAS ${MODULE_NAME})

target_include_directories(${MODULE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${MODULE_NAME} PUBLIC ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaclient
                                            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq
)
