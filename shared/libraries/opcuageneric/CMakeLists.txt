opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

message(STATUS "${OPENDAQ_SDK_NAME} version: ${OPENDAQ_PACKAGE_VERSION}")
add_compile_definitions(OPENDAQ_OPCUA_PACKAGE_VERSION="${OPENDAQ_PACKAGE_VERSION}")

if (${REPO_OPTION_PREFIX}_ENABLE_GENERIC_CLIENT)
    add_subdirectory(opcuageneric_client)
endif()

if (${REPO_OPTION_PREFIX}_ENABLE_SIMPLE_SERVER)
    add_subdirectory(opcua_simple_objects)
    add_subdirectory(opcua_simple_server)
endif()

if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    #add_subdirectory(tests)
endif()
