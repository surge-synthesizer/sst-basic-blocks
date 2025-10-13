
find_package(Sndfile)

add_executable(sweep-test sweep-test.cpp)
target_link_libraries(sweep-test sst-basic-blocks)
target_include_directories(sweep-test PRIVATE  ${simde_SOURCE_DIR})