add_library(robot_protocol
        wheel_velocity_codec.c
)

target_include_directories(robot_protocol
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()