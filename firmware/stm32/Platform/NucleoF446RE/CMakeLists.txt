add_library(stm32_platform STATIC EXCLUDE_FROM_ALL
        Core/Src/motor_driver_port_stm32.c
)

target_compile_features(stm32_platform PUBLIC c_std_99)

target_compile_definitions(stm32_platform
        PRIVATE
        USE_HAL_DRIVER
        STM32F446xx
)

target_include_directories(stm32_platform
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
)

target_include_directories(stm32_platform
        SYSTEM PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
        ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
)

target_link_libraries(stm32_platform
        PUBLIC
        stm32_core
)

if(CMAKE_CROSSCOMPILING)
    add_executable(NucleoF446RE
            Core/Startup/startup_stm32f446retx.s
            Core/Src/gpio.c
            Core/Src/main.c
            Core/Src/stm32f4xx_hal_msp.c
            Core/Src/stm32f4xx_it.c
            Core/Src/syscalls.c
            Core/Src/sysmem.c
            Core/Src/system_stm32f4xx.c
            Core/Src/tim.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
            Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
    )

    set_target_properties(NucleoF446RE PROPERTIES
            SUFFIX .elf
    )

    target_compile_features(NucleoF446RE PRIVATE c_std_99)

    target_compile_definitions(NucleoF446RE
            PRIVATE
            USE_HAL_DRIVER
            STM32F446xx
    )

    target_include_directories(NucleoF446RE
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/Core/Inc
    )

    target_include_directories(NucleoF446RE
            SYSTEM PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
            ${CMAKE_CURRENT_LIST_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
            ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
            ${CMAKE_CURRENT_LIST_DIR}/Drivers/CMSIS/Include
    )

    target_compile_options(NucleoF446RE
            PRIVATE
            -ffunction-sections
            -fdata-sections
    )

    target_link_libraries(NucleoF446RE
            PRIVATE
            stm32_core
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
