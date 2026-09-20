add_library(stm32_core STATIC
        Src/motion_stop.c
)

target_compile_features(stm32_core PUBLIC c_std_99)

target_include_directories(stm32_core
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/Inc
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()