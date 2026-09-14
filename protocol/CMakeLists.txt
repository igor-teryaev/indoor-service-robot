add_library(robot_protocol
        byte_codec.c
        wheel_velocity_codec.c
)

target_compile_features(robot_protocol PUBLIC c_std_99)

target_include_directories(robot_protocol
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
