set(MODULE_NAME opcuatms_test_utils)

set(MODULE_SOURCES tms_object_test.h
                   tms_object_test.cpp
                   test_input_port_notifications.h
                   test_input_port_notifications.cpp
    
)

add_library(${MODULE_NAME} STATIC ${MODULE_SOURCES})
add_library(${OPENDAQ_SDK_TARGET_NAMESPACE}::${MODULE_NAME} ALIAS ${MODULE_NAME})

target_include_directories(${MODULE_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${MODULE_NAME} PUBLIC ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq_test_utils gtest gmock
                                            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaclient
                                            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaserver
                                            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq
                                            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcua_daq_types
)
