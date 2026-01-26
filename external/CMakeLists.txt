set(CMAKE_FOLDER external)
list(APPEND CMAKE_MESSAGE_CONTEXT external)

opendaq_setup_module_default_dependencies(${REPO_OPTION_PREFIX})

add_subdirectory(open62541)
add_subdirectory(opcua_daq_types)
