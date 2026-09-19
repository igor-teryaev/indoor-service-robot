cmake_minimum_required(VERSION 3.24)

project(indoor_service_robot_stm32
        LANGUAGES C ASM
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

add_compile_options(
        -mcpu=cortex-m4
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
)

add_link_options(
        -mcpu=cortex-m4
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
)

add_subdirectory(
        ${CMAKE_CURRENT_LIST_DIR}/../../protocol
        ${CMAKE_CURRENT_BINARY_DIR}/protocol
)