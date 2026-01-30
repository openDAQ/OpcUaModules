opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

add_subdirectory(opcuatms)
add_subdirectory(opcuatms_server)
add_subdirectory(opcuatms_client)

if (${REPO_OPTION_PREFIX}_BUILDING_AS_SUBMODULE AND OPENDAQ_ENABLE_TESTS OR ${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    add_subdirectory(tests)
endif()
