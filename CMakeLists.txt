cmake_minimum_required(VERSION 3.10)
project(sst-basic-blocks VERSION 0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)

if (${SST_BASIC_BLOCKS_BUILD_TESTS})
    include(cmake/CPM.cmake)

    if (NOT TARGET fmt)
        CPMAddPackage(NAME fmt
                GITHUB_REPOSITORY fmtlib/fmt
                GIT_TAG master
                )
    endif()

    add_executable(sst-basic-blocks-test
            tests/smoketest.cpp
            tests/dsp_tests.cpp
            tests/simd_tests.cpp
            tests/block_tests.cpp
            tests/table_tests.cpp
            tests/param_tests.cpp
            )
    if (NOT TARGET simde)
         message(STATUS "Importing SIMDE with CPM")
         CPMAddPackage(NAME simde
                GITHUB_REPOSITORY simd-everywhere/simde
                VERSION 0.7.2
                )
         target_include_directories(sst-basic-blocks-test PRIVATE
                 ${simde_SOURCE_DIR}
                 tests)
    else()
          target_link_libraries(sst-basic-blocks-test PRIVATE simde)
    endif()

    target_link_libraries(sst-basic-blocks-test PRIVATE fmt ${PROJECT_NAME})
    target_compile_definitions(sst-basic-blocks-test PUBLIC _USE_MATH_DEFINES=1)
    target_compile_definitions(sst-basic-blocks-test PRIVATE CATCH_CONFIG_DISABLE_EXCEPTIONS=1)

endif ()
