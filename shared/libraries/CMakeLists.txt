opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

add_subdirectory(opcua)

if (${REPO_OPTION_PREFIX}_ENABLE_CLIENT OR ${REPO_OPTION_PREFIX}_ENABLE_SERVER)
    add_subdirectory(opcuatms)
endif()

if (${REPO_OPTION_PREFIX}_ENABLE_GENERIC_CLIENT)
    add_subdirectory(opcuageneric)
endif()

