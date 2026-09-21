add_executable(stm32_core_tests
        motion_stop_test.cpp
)

add_executable(motor_driver_drv8833_tests
        motor_driver_drv8833_test.cpp
)

set(STM32_CORE_TEST_TARGETS
        stm32_core_tests
        motor_driver_drv8833_tests
)

include(GoogleTest)

foreach(test_target IN LISTS STM32_CORE_TEST_TARGETS)
    target_compile_features(
            ${test_target}
            PRIVATE
            cxx_std_20
    )

    target_link_libraries(
            ${test_target}
            PRIVATE
            stm32_core
            GTest::gtest_main
    )

    gtest_discover_tests(${test_target})
endforeach()