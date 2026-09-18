cmake_minimum_required(VERSION 3.15)

project(EmbeddedToolkits_Test)
set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE EmbeddedToolkits_Test_sources src/*.cpp)
add_executable(EmbeddedToolkits_Test ${EmbeddedToolkits_Test_sources})

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(googletest)

target_link_libraries(
    EmbeddedToolkits_Test
    GTest::gtest_main
)
