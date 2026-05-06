cmake_minimum_required(VERSION 3.10)
opendaq_set_cmake_folder_context(TARGET_FOLDER_NAME)
project(opcua_simple_objects VERSION ${${REPO_OPTION_PREFIX}_VERSION} LANGUAGES CXX)

add_subdirectory(src)