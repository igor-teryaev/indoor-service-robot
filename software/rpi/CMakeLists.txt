cmake_minimum_required(VERSION 3.20)

project(indoor_service_robot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(
        robot_core
        src/control_state.cpp
        src/safety_state.cpp
        src/motion_coordinator.cpp
        src/motion_watchdog.cpp
        src/differential_drive_kinematics.cpp
)

target_include_directories(
        robot_core
        PUBLIC
        include
)

add_executable(
        indoor_service_robot
        src/main.cpp
)

target_link_libraries(
        indoor_service_robot
        PRIVATE
        robot_core
)

include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_subdirectory(tests)