set(LIB_NAME opcua_simple_server)
set(HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

set(SRC_Cpp simple_server.cpp
)

set(SRC_PublicHeaders
)

set(SRC_PrivateHeaders simple_server.h
)


set(SRC_PublicHeaders ${SRC_PublicHeaders})
set(SRC_Cpp ${SRC_Cpp})

source_group("server" FILES ${HEADERS_DIR}/simple_server.h
                            simple_server.cpp
)

opendaq_prepend_include(${LIB_NAME} SRC_PrivateHeaders)
opendaq_prepend_include(${LIB_NAME} SRC_PublicHeaders)


add_library(${LIB_NAME} STATIC ${SRC_Cpp}
                               ${SRC_PublicHeaders}
                               ${SRC_PrivateHeaders}
)

add_library(${OPENDAQ_SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if(BUILD_64Bit OR BUILD_ARM)
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>

                                              $<INSTALL_INTERFACE:include>
)

target_link_libraries(${LIB_NAME} PUBLIC ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaserver
                                         ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq
                                         ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcua_simple_objects
)

set_target_properties(${LIB_NAME} PROPERTIES PUBLIC_HEADER "${SRC_PublicHeaders}")

opendaq_set_output_lib_name(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
