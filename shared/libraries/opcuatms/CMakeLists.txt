opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

message(STATUS "${OPENDAQ_SDK_NAME} version: ${OPENDAQ_PACKAGE_VERSION}")
add_compile_definitions(OPENDAQ_OPCUA_PACKAGE_VERSION="${OPENDAQ_PACKAGE_VERSION}")

add_subdirectory(opcuatms)
add_subdirectory(opcuatms_server)
add_subdirectory(opcuatms_client)

if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
