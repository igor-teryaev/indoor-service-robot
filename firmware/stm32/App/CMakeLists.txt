add_library(stm32_app STATIC
        Src/motion_stop.c
        Src/motor_driver_dri0041.c
        Src/motion_watchdog.c
        Src/motion_command_guard.c
        Src/uart_rx_queue.c
        Src/uart_protocol_receiver.c
        Src/uart_protocol_transmitter.c
        Src/uart_link_manager.c
        Src/stm32_motion_protocol_manager.c
        Src/wheel_encoder.c
        Src/wheel_velocity_feedforward.c
)

target_compile_features(stm32_app PUBLIC c_std_99)

target_include_directories(stm32_app
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Inc
)

target_link_libraries(stm32_app
        PUBLIC
        robot_protocol
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
