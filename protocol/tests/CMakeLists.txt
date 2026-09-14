add_executable(robot_protocol_tests
        byte_codec_test.cpp
        wheel_velocity_codec_test.cpp
)

target_compile_features(
        robot_protocol_tests
        PRIVATE
        cxx_std_20
)

target_link_libraries(robot_protocol_tests
        PRIVATE
        robot_protocol
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(robot_protocol_tests)
