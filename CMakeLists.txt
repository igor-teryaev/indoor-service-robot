cmake_minimum_required(VERSION 3.24)

project(indoor_service_robot LANGUAGES C CXX)

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)
endif()

add_subdirectory(protocol)
add_subdirectory(software/rpi)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()