cmake_minimum_required(VERSION 4.2)
project(cloud_core)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

if(NOT DEFINED SHARED_SOURCES_CMAKE_PATH)
    FetchContent_Declare(
            shared_core
            GIT_REPOSITORY "https://github.com/amqhi/shared-core.git"
            GIT_TAG        main
    )
    FetchContent_MakeAvailable(shared_core)
    set(SHARED_SOURCES_CMAKE_PATH "${shared_core_SOURCE_DIR}/sources.cmake")
endif()

include("${SHARED_SOURCES_CMAKE_PATH}")
include("sources.cmake")

add_library(
        cloud_core
        STATIC
        ${CLOUD_BRIDGE_SOURCES}
        ${SHARED_SOURCES}
        ${CLOUD_SOURCES}
        ${SHARED_JAVA_BINDING_BINDING_SOURCES}
        ${CLOUD_JAVA_BINDING_BINDING_SOURCES}
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${SHARED_INCLUDE_DIRS} ${CLOUD_INCLUDE_DIRS} ${SHARED_JAVA_BINDING_INCLUDE_DIRS} ${CLOUD_JAVA_BINDING_INCLUDE_DIRS})
