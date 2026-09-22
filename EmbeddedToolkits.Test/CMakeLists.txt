cmake_minimum_required(VERSION 3.15)

project(EmbeddedToolkits_Test)

enable_testing()

file(
    GLOB_RECURSE
    EmbeddedToolkits_Test_sources
    CONFIGURE_DEPENDS
    src/*.cpp
)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
)
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

foreach(cxx_standard IN ITEMS 11 14 17 20)
    set(target_name EmbeddedToolkits_Test_CXX${cxx_standard})

    add_executable(${target_name} ${EmbeddedToolkits_Test_sources})
    set_target_properties(
        ${target_name}
        PROPERTIES
            CXX_STANDARD ${cxx_standard}
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
    )

    target_link_libraries(
        ${target_name}
        EmbeddedToolkits
        GTest::gtest_main
    )

    gtest_discover_tests(
        ${target_name}
        TEST_PREFIX CXX${cxx_standard}.
    )
endforeach()
