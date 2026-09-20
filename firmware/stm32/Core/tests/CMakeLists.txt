add_executable(stm32_core_tests
        motion_stop_test.cpp
)

target_compile_features(
        stm32_core_tests
        PRIVATE
        cxx_std_20
)

target_link_libraries(
        stm32_core_tests
        PRIVATE
        stm32_core
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(stm32_core_tests)