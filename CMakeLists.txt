set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.25)

set(REPO_NAME OpcUaModules)
set(REPO_OPTION_PREFIX DAQMODULES_OPCUA)

add_subdirectory(cmake)

opendaq_setup_module_project(${REPO_OPTION_PREFIX} ${REPO_NAME})
opendaq_setup_module_subfolders(${REPO_OPTION_PREFIX} ${REPO_NAME})
