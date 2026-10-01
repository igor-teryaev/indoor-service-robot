set(STM32_PLATFORM_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
)

set(STM32_HAL_INCLUDE_DIRS
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
)

add_library(stm32_platform STATIC EXCLUDE_FROM_ALL
        Port/motor_driver_dri0041_port_stm32.c
        Port/uart_rx_port_stm32.c
        Port/uart_tx_port_stm32.c
)

# Keep CubeMX main.c in CLion's host code model without compiling HAL firmware.
if(NOT CMAKE_CROSSCOMPILING)
    set_source_files_properties(
            Core/Src/main.c
            PROPERTIES HEADER_FILE_ONLY TRUE
    )

    target_sources(stm32_platform PRIVATE Core/Src/main.c)
endif()

target_compile_features(stm32_platform PUBLIC c_std_99)

target_compile_definitions(stm32_platform
        PRIVATE
        USE_HAL_DRIVER
        STM32F446xx
)

target_include_directories(stm32_platform
        PRIVATE
        ${STM32_PLATFORM_INCLUDE_DIRS}
)

target_include_directories(stm32_platform
        SYSTEM PRIVATE
        ${STM32_HAL_INCLUDE_DIRS}
)

target_link_libraries(stm32_platform PUBLIC stm32_app)

if(CMAKE_CROSSCOMPILING)
    file(GLOB STM32_CUBE_SOURCES CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_LIST_DIR}/Core/Src/*.c
            ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/*.c
    )

    add_executable(NucleoF446RE
            Core/Startup/startup_stm32f446retx.s
            ${STM32_CUBE_SOURCES}
    )

    set_target_properties(NucleoF446RE PROPERTIES SUFFIX .elf)

    target_compile_features(NucleoF446RE PRIVATE c_std_99)

    target_compile_definitions(NucleoF446RE
            PRIVATE
            USE_HAL_DRIVER
            STM32F446xx
    )

    target_include_directories(NucleoF446RE
            PRIVATE
            ${STM32_PLATFORM_INCLUDE_DIRS}
    )

    target_include_directories(NucleoF446RE
            SYSTEM PRIVATE
            ${STM32_HAL_INCLUDE_DIRS}
    )

    target_compile_options(NucleoF446RE
            PRIVATE
            -ffunction-sections
            -fdata-sections
    )

    target_link_libraries(NucleoF446RE
            PRIVATE
            stm32_app
            stm32_platform
            c
            m
    )

    target_link_options(NucleoF446RE
            PRIVATE
            --specs=nosys.specs
            --specs=nano.specs
            -static
            -T${CMAKE_CURRENT_LIST_DIR}/STM32F446RETX_FLASH.ld
            -Wl,--gc-sections
            -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/NucleoF446RE.map
    )
endif()
