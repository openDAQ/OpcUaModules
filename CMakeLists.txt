set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
cmake_minimum_required(VERSION 3.25)

set(REPO_NAME OpcUaModules)
set(REPO_OPTION_PREFIX DAQMODULES_OPCUA)

list(APPEND CMAKE_MESSAGE_CONTEXT ${REPO_NAME})

add_subdirectory(cmake)

opendaq_read_file_contents("${CMAKE_CURRENT_LIST_DIR}/module_version" module_version)
opendaq_get_version_major_minor_patch("${module_version}" ${REPO_OPTION_PREFIX}_VERSION)

# 32-bit Linux cross-compilation setup (must be before any project())
if(NOT DEFINED PROJECT_SOURCE_DIR AND OPENDAQ_FORCE_COMPILE_32BIT AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    opendaq_32bit_build_linux_early_setup()
endif()
opendaq_common_early_setup()

project(${REPO_NAME} VERSION ${${REPO_OPTION_PREFIX}_VERSION} LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(${REPO_OPTION_PREFIX}_BUILDING_AS_SUBMODULE OFF CACHE INTERNAL "Building ${REPO_NAME} standalone")
    message(STATUS "Building ${REPO_NAME} version ${${REPO_OPTION_PREFIX}_VERSION} standalone")
else()
    set(${REPO_OPTION_PREFIX}_BUILDING_AS_SUBMODULE ON CACHE INTERNAL "Building ${REPO_NAME} as submodule")
    message(STATUS "Building ${REPO_NAME} version ${${REPO_OPTION_PREFIX}_VERSION} as submodule")
endif()

# options
opendaq_setup_common_build_options()
opendaq_setup_project_specific_build_options(${REPO_OPTION_PREFIX})
option(${REPO_OPTION_PREFIX}_ENABLE_EXAMPLE_APP "Enable ${REPO_NAME} example applications" ON)
option(${REPO_OPTION_PREFIX}_ENABLE_TESTS "Enable ${REPO_NAME} testing" ON)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ModuleOptions.cmake" OPTIONAL)

opendaq_common_compile_targets_settings()
opendaq_setup_compiler_flags(${REPO_OPTION_PREFIX})

if (${REPO_OPTION_PREFIX}_ENABLE_TESTS)
    message(STATUS "Unit tests are ENABLED")
    enable_testing()
else()
    message(STATUS "Unit tests are DISABLED")
endif()

add_subdirectory(external)

if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "openDAQ package version: ${OPENDAQ_PACKAGE_VERSION}")
    add_compile_definitions(OPENDAQ_PACKAGE_VERSION="${OPENDAQ_PACKAGE_VERSION}")
endif()

add_subdirectory(shared)
add_subdirectory(modules)
