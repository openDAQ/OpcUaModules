set(LIB_NAME opcuageneric_client)
set(HEADERS_DIR ../include/${TARGET_FOLDER_NAME})


set(SRC_PublicHeaders   constants.h
                        opcuageneric.h
                        generic_client_device_impl.h
                        opcua_monitored_item_fb_impl.h
                        status_container.h
                        status_adaptor.h
)
set(SRC_Cpp generic_client_device_impl.cpp
            opcua_monitored_item_fb_impl.cpp
)

source_group("common" FILES ${HEADERS_DIR}/constants.h
                            ${HEADERS_DIR}/opcuageneric.h
                            ${HEADERS_DIR}/status_container.h
)
source_group("device" FILES ${HEADERS_DIR}/generic_client_device_impl.h
                            generic_client_device_impl.cpp
)
source_group("function_block" FILES ${HEADERS_DIR}/opcua_monitored_item_fb_impl.h
                            opcua_monitored_item_fb_impl.cpp
)

opendaq_prepend_include(${LIB_NAME} SRC_PublicHeaders)

add_library(${LIB_NAME} STATIC ${SRC_Cpp}
                               ${SRC_PublicHeaders}
)

add_library(${OPENDAQ_SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if(BUILD_64Bit OR BUILD_ARM)
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE OFF)
endif()

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /bigobj)
elseif (MINGW AND CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${LIB_NAME} PRIVATE -Wa,-mbig-obj)
endif()

target_link_libraries(${LIB_NAME}
        PUBLIC
            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opcuaclient
            ${OPENDAQ_SDK_TARGET_NAMESPACE}::opendaq
)

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>

                                              $<INSTALL_INTERFACE:include>
)

set_target_properties(${LIB_NAME} PROPERTIES PUBLIC_HEADER "${SRC_PublicHeaders}")

opendaq_set_output_lib_name(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
