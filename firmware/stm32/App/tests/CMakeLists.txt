add_executable(stm32_app_tests
        motion_stop_test.cpp
)

add_executable(motor_driver_drv8833_tests
        motor_driver_drv8833_test.cpp
)

add_executable(motion_watchdog_tests
        motion_watchdog_test.cpp
)

add_executable(motion_command_guard_tests
        motion_command_guard_test.cpp
)

add_executable(uart_rx_queue_tests
        uart_rx_queue_test.cpp
)

add_executable(uart_protocol_receiver_tests
        uart_protocol_receiver_test.cpp
)

add_executable(uart_protocol_transmitter_tests
        uart_protocol_transmitter_test.cpp
)

add_executable(uart_link_manager_tests
        uart_link_manager_test.cpp
)

add_executable(stm32_motion_protocol_manager_tests
        stm32_motion_protocol_manager_test.cpp
)

add_executable(wheel_velocity_feedforward_tests
        wheel_velocity_feedforward_test.cpp
)

set(STM32_APP_TEST_TARGETS
        stm32_app_tests
        motor_driver_drv8833_tests
        motion_watchdog_tests
        motion_command_guard_tests
        uart_rx_queue_tests
        uart_protocol_receiver_tests
        uart_protocol_transmitter_tests
        uart_link_manager_tests
        stm32_motion_protocol_manager_tests
        wheel_velocity_feedforward_tests
)

include(GoogleTest)

foreach(test_target IN LISTS STM32_APP_TEST_TARGETS)
    target_compile_features(
            ${test_target}
            PRIVATE
            cxx_std_20
    )

    target_link_libraries(
            ${test_target}
            PRIVATE
            stm32_app
            GTest::gtest_main
    )

    gtest_discover_tests(${test_target})
endforeach()
