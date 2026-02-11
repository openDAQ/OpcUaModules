include(FetchContent)

find_package(${OPENDAQ_SDK_NAME} GLOBAL)
if (NOT ${OPENDAQ_SDK_NAME}_FOUND)
    set(OPENDAQ_ENABLE_TESTS OFF CACHE BOOL "" FORCE)

    opendaq_read_file_contents("${CMAKE_CURRENT_LIST_DIR}/opendaq_ref" OPENDAQ_REF)

    FetchContent_Declare(
        ${OPENDAQ_SDK_NAME}
        GIT_REPOSITORY https://github.com/openDAQ/openDAQ.git
        GIT_TAG ${OPENDAQ_REF}
        GIT_PROGRESS   ON
        SOURCE_DIR ${FETCHCONTENT_EXTERNALS_DIR}/src/${OPENDAQ_SDK_NAME}
    )
    FetchContent_MakeAvailable(${OPENDAQ_SDK_NAME})
else()
    message(STATUS "Found installed openDAQ version: ${${OPENDAQ_SDK_NAME}_VERSION}")
    set(OPENDAQ_PACKAGE_VERSION "${${OPENDAQ_SDK_NAME}_VERSION}" CACHE INTERNAL "openDAQ SDK version")
endif()

message(STATUS "openDAQ package version: ${OPENDAQ_PACKAGE_VERSION}")
