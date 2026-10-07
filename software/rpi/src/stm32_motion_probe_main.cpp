#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "stm32_client_runner.h"

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: stm32_motion_probe <serial-device>"
            << std::endl;

        return 1;
    }

    Stm32ClientRunner runner{
        std::string(argv[1])
    };

    std::cout
        << "Waiting for STM32 synchronization..."
        << std::endl;

    while (!runner.is_synchronized())
    {
        runner.poll();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    std::cout
        << "STM32 synchronized"
        << std::endl;


    const WheelVelocityCommand start_command =
    {
        .left_velocity_mm_s = 42,   // ~15% PWM
        .right_velocity_mm_s = 42
    };

    const WheelVelocityCommand sustain_command =
    {
        .left_velocity_mm_s = 21,   // ~7.5% PWM
        .right_velocity_mm_s = 21
    };

    const WheelVelocityCommand stop_command = {
        .left_velocity_mm_s = 0,
        .right_velocity_mm_s = 0
    };

    std::cout
        << "Refreshing low-power LEFT wheel demand for 1 second"
        << std::endl;

    const auto startup_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);

    auto next_command_refresh = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() <  startup_deadline)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= next_command_refresh)
        {
            runner.set_wheel_command(start_command);

            next_command_refresh = now + std::chrono::milliseconds(100);
        }

        runner.poll();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    const auto sustain_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    next_command_refresh = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() <  sustain_deadline)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= next_command_refresh)
        {
            runner.set_wheel_command(sustain_command);

            next_command_refresh = now + std::chrono::milliseconds(100);
        }

        runner.poll();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }


    std::cout
        << "Sending explicit zero command"
        << std::endl;

    runner.set_wheel_command(
        stop_command);

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(2000);

    while (std::chrono::steady_clock::now() <
           stop_deadline)
    {
        runner.poll();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    std::cout
        << "Probe complete"
        << std::endl;
    return 0;
}