#include <chrono>
#include <iostream>
#include <array>

#include "crsf_channels.h"
#include "crsf_frame_decoder.h"
#include "linux_serial_port.h"
#include "manual_input_guard.h"
#include "manual_control_config.h"
#include "manual_control_input.h"

int main(
    int argc,
    char* argv[])
{
    if (argc != 2)
    {
        std::cerr
            << "Usage: crsf_receiver_probe <serial-device>"
            << std::endl;

        return 1;
    }

    LinuxSerialPort serial_port;

    if (!serial_port.open(argv[1], 420000U))
    {
        std::cerr
            << "Error opening serial port " << argv[1]
            << std::endl;
        return 1;
    }

    std::cout
        << "Receiver port " << argv[1] << " opened"
        << std::endl;

    CrsfFrameDecoder decoder;
    CrsfFrame frame{};
    CrsfChannels channels{};
    std::array<std::uint8_t, 256> buffer{};

    ManualInputGuard guard;
    bool previous_permission = false;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::minutes(5);

    std::cout
                    << "Manual permission: BLOCKED"
                    << std::endl;

    auto next_input_print = ManualInputGuard::Clock::now();

    while (std::chrono::steady_clock::now() <  deadline)
    {
        const bool permission = guard.poll(ManualInputGuard::Clock::now());
        if (permission != previous_permission)
        {
            std::cout
                << "Manual permission: "
                << (permission ? "ENABLED" : "BLOCKED")
                << std::endl;

            previous_permission = permission;
        }

        LinuxSerialPollResult result = serial_port.wait(false, 50);
        if (result.disconnected)
        {
            std::cerr
                << "Receiver port " << argv[1] << " disconnected"
                << std::endl;
            return 1;
        }
        if (!result.readable)
        {
            continue;
        }

        const auto bytes_read = serial_port.read_some(buffer.data(), buffer.size());
        if (bytes_read < 0)
        {
            std::cerr
                << "Error reading from serial port " << argv[1]
                << std::endl;
            return 1;
        }

        if (bytes_read > 0)
        {
            for (std::size_t i = 0; i < static_cast<std::size_t>(bytes_read); ++i)
            {
                // Передаємо кожен байт у декодер
                if (decoder.feed_byte(buffer[i], frame)) {

                    // Декодер успішно зібрав та випустив повний кадр CrsfFrame
                    // 2. Викликаємо функцію розпакування каналів
                    if (crsf_channels_decode(frame, channels)) {
                        const auto received_at = ManualInputGuard::Clock::now();
                        guard.on_channels(channels, received_at);

                        if (received_at >= next_input_print)
                        {
                            const double forward = normalize_manual_stick(
                                channels.values[manual_control_config::forward_channel]);

                            const double steering = normalize_manual_stick(
                                channels.values[manual_control_config::steering_channel]);

                            const auto wheels = mix_manual_sticks(forward, steering);

                            std::cout
                                << "Stick input: forward=" << forward
                                << ", steering=" << steering
                                << " | Wheels: left=" << wheels.left
                                << ", right=" << wheels.right
                                << std::endl;

                            next_input_print =
                                received_at + std::chrono::milliseconds{200};
                        }
                    }
                }
            }
        }
    }
    return 0;
}
