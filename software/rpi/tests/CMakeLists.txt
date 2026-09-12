add_executable(
        indoor_service_robot_tests
        control_state_test.cpp
        safety_state_test.cpp
        motion_coordinator_test.cpp
        motion_watchdog_test.cpp
        differential_drive_kinematics_test.cpp
)

target_link_libraries(
        indoor_service_robot_tests
        PRIVATE
        robot_core
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(indoor_service_robot_tests)