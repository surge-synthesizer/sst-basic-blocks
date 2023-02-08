cmake_minimum_required(VERSION 3.10)
project(sst-basic-blocks VERSION 0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

if (${SST_BASIC_BLOCKS_BUILD_TESTS})
    include(cmake/CPM.cmake)
    message(STATUS "Importing SIMDE with CPM")
    CPMAddPackage(NAME simde
            GITHUB_REPOSITORY simd-everywhere/simde
            VERSION 0.7.2
            )
    add_library(simde INTERFACE)
    target_include_directories(simde INTERFACE ${simde_SOURCE_DIR})

    add_executable(smoke-test tests/smoketest.cpp)
    target_link_libraries(smoke-test PUBLIC simde ${PROJECT_NAME})
endif()