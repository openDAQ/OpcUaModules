set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.25)

set(REPO_NAME OpcUaModules)
set(REPO_OPTION_PREFIX DAQMODULES_OPCUA)

add_subdirectory(cmake)

opendaq_setup_repo_version("${REPO_OPTION_PREFIX}" ${REPO_NAME} "module_version")

if(NOT DEFINED ${REPO_OPTION_PREFIX}_VERSION)
    message(FATAL_ERROR "Module project version is not specified - specify module version using -D${${REPO_OPTION_PREFIX}_VERSION} or via module_version file in module project root dir")
endif()

project(${REPO_NAME} VERSION ${${REPO_OPTION_PREFIX}_VERSION} LANGUAGES CXX)

opendaq_module_common_setup(${REPO_OPTION_PREFIX} ${REPO_NAME})

add_subdirectory(external)

if (NOT ${REPO_OPTION_PREFIX}_BUILDING_AS_SUBMODULE)
    message(STATUS "openDAQ package version: ${OPENDAQ_PACKAGE_VERSION}")
    add_compile_definitions(OPENDAQ_PACKAGE_VERSION="${OPENDAQ_PACKAGE_VERSION}")
endif()

add_subdirectory(shared)
add_subdirectory(modules)
