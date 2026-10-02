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
        .left_velocity_mm_s = 1,
        .right_velocity_mm_s = 0
    };

    std::cout
        << "Sending one low-power LEFT wheel demand"
        << std::endl;

    runner.set_wheel_command(
        command);

    const auto probe_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(600);

    while (std::chrono::steady_clock::now() <
           probe_deadline)
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