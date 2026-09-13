cmake_minimum_required(VERSION 3.24)

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
        src/wheel_velocity_command_converter.cpp
)
target_link_libraries(
        robot_core
        PUBLIC
        robot_protocol
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

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

target_link_libraries(
        indoor_service_robot
        PRIVATE
        robot_core
)