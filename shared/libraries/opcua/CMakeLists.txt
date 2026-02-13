opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)

add_subdirectory(opcuashared)
add_subdirectory(opcuaserver)
add_subdirectory(opcuaclient)

if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    add_subdirectory(tests)
endif()

#GENERATE_DOCUMENTATION(
#    NAME "OpcUa"
#    INPUT
#        opcuashared/include
#        opcuaserver/include
#        opcuaclient/include
#)
