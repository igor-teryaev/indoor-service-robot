cmake_minimum_required(VERSION 3.24)

project(indoor_service_robot LANGUAGES C)

if(CMAKE_CROSSCOMPILING)
    enable_language(ASM)
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
else()
    enable_language(CXX)
    include(CTest)

    if(BUILD_TESTING)
        include(FetchContent)

        FetchContent_Declare(
                googletest
                URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )

        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

add_subdirectory(protocol)
add_subdirectory(firmware/stm32/App)

if(CMAKE_CROSSCOMPILING)
    add_subdirectory(firmware/stm32/Platform/NucleoF446RE)
else()
    add_subdirectory(software/rpi)
    add_subdirectory(
            firmware/stm32/Platform/NucleoF446RE
            EXCLUDE_FROM_ALL
    )

    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
