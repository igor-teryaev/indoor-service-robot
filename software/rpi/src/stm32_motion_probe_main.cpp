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
    << "Motors running. Press Nucleo RESET within 5 seconds."
    << std::endl;

const auto reset_window_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::seconds(5);

auto next_command_refresh =
    std::chrono::steady_clock::now();

bool link_was_lost = false;

while (std::chrono::steady_clock::now() <
       reset_window_deadline)
{
    const auto now =
        std::chrono::steady_clock::now();

    if (runner.is_synchronized() &&
        now >= next_command_refresh)
    {
        runner.set_wheel_command(
            command);

        next_command_refresh =
            now +
            std::chrono::milliseconds(100);
    }

    runner.poll();

    if (!runner.is_synchronized())
    {
        link_was_lost = true;
        break;
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(10));
}

if (!link_was_lost)
{
    std::cout
        << "No link loss detected. Stopping probe safely."
        << std::endl;

    const WheelVelocityCommand zero_command =
    {
        .left_velocity_mm_s = 0,
        .right_velocity_mm_s = 0
    };

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

    return 0;
}

std::cout
    << "Link loss detected. No more wheel demand will be supplied."
    << std::endl;

const auto reconnect_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::seconds(5);

while (!runner.is_synchronized() &&
       std::chrono::steady_clock::now() <
           reconnect_deadline)
{
    runner.poll();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(10));
}

if (!runner.is_synchronized())
{
    std::cerr
        << "STM32 did not resynchronize"
        << std::endl;

    return 1;
}

std::cout
    << "STM32 resynchronized. Verifying no old motion is replayed..."
    << std::endl;

const auto observation_deadline =
    std::chrono::steady_clock::now() +
    std::chrono::seconds(1);

while (std::chrono::steady_clock::now() <
       observation_deadline)
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