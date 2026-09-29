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

    const WheelVelocityCommand command =
    {
        .left_velocity_mm_s = 100,
        .right_velocity_mm_s = 100
    };

    std::cout
        << "Refreshing 100/100 wheel demand for 1 second"
        << std::endl;

    const auto motion_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::seconds(1);

    auto next_command_refresh =
        std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() <
           motion_deadline)
    {
        const auto now =
            std::chrono::steady_clock::now();

        if (now >= next_command_refresh)
        {
            runner.set_wheel_command(
                command);

            next_command_refresh =
                now +
                std::chrono::milliseconds(100);
        }

        runner.poll();

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10));
    }

    const WheelVelocityCommand zero_command =
    {
        .left_velocity_mm_s = 0,
        .right_velocity_mm_s = 0
    };

    std::cout
        << "Sending explicit zero command"
        << std::endl;

    runner.set_wheel_command(
        zero_command);

    const auto stop_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(500);

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