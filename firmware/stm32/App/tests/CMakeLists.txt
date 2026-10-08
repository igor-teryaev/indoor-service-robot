add_executable(stm32_app_tests
        motion_stop_test.cpp
)

add_executable(motor_driver_drv8833_tests
        motor_driver_drv8833_test.cpp
        ../Src/motor_driver_drv8833.c
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

add_executable(stm32_motion_stop_integration_tests
        stm32_motion_stop_integration_test.cpp
)

include(GoogleTest)

target_compile_features(
        motor_driver_drv8833_tests
        PRIVATE
        c_std_99
        cxx_std_20
)

target_include_directories(
        motor_driver_drv8833_tests
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../Inc
)

target_link_libraries(
        motor_driver_drv8833_tests
        PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(motor_driver_drv8833_tests)


add_executable(motor_driver_dri0041_tests
        motor_driver_dri0041_test.cpp
        ../Src/motor_driver_dri0041.c
)
target_compile_features(
        motor_driver_dri0041_tests
        PRIVATE
        c_std_99
        cxx_std_20
)
target_include_directories(
        motor_driver_dri0041_tests
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../Inc
)
target_link_libraries(
        motor_driver_dri0041_tests
        PRIVATE
        GTest::gtest_main
)

gtest_discover_tests(motor_driver_dri0041_tests)

set(STM32_APP_TEST_TARGETS
        stm32_app_tests
        motion_watchdog_tests
        motion_command_guard_tests
        uart_rx_queue_tests
        uart_protocol_receiver_tests
        uart_protocol_transmitter_tests
        uart_link_manager_tests
        stm32_motion_protocol_manager_tests
        stm32_motion_stop_integration_tests
        wheel_velocity_feedforward_tests
)

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
