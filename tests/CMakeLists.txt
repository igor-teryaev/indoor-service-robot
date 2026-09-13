add_executable(integration_tests
        wheel_velocity_protocol_integration_test.cpp
)

target_compile_features(
        integration_tests
        PRIVATE
        cxx_std_20
)

target_link_libraries(
        integration_tests
        PRIVATE
        robot_core
        robot_protocol
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(integration_tests)