#include <iostream>
#include <chrono>
#include <thread>
#include <array>

#include "crsf_channels.h"
#include "crsf_frame_decoder.h"
#include "linux_serial_port.h"
#include "manual_input_guard.h"
#include "stm32_client_runner.h"
#include "manual_control_config.h"
#include "manual_control_input.h"

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr
            << "Usage: manual_control_probe <ELRS-device> <STM32-device>\n";
        return 1;
    }

    LinuxSerialPort receiver_port;
    if (!receiver_port.open(argv[1], 420000U))
    {
        std::cerr << "Cannot open ELRS port: " << argv[1] << '\n';
        return 1;
    }

    Stm32ClientRunner runner{argv[2]};
    CrsfFrameDecoder decoder;
    ManualInputGuard guard;

    bool previously_synchronized = false;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::minutes{5};

    std::array<std::uint8_t, 256> buffer{};
    CrsfFrame frame{};
    CrsfChannels channels{};

    auto last_receiver_service_at = ManualInputGuard::Clock::now();

    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto service_at = ManualInputGuard::Clock::now();

        const bool recovering_receiver =
            service_at - last_receiver_service_at >=
            std::chrono::milliseconds{200};

        last_receiver_service_at = service_at;

        if (recovering_receiver)
        {
            runner.set_wheel_command(WheelVelocityCommand{});
            guard.reset();
            decoder.reset();
        }

        bool received_channels = false;
        WheelVelocityCommand candidate_command{};

        bool receiver_drained = false;

        for (unsigned batch = 0; batch < 32U; ++batch)
        {
            const auto bytes_read =
                receiver_port.read_some(buffer.data(), buffer.size());

            if (bytes_read < 0)
            {
                std::cerr << "ELRS serial read failed\n";
                break;
            }

            if (bytes_read == 0)
            {
                receiver_drained = true;
                break;
            }

            if (recovering_receiver)
            {
                // Discard buffered bytes without decoding them.
                continue;
            }

            for (std::size_t i = 0;
                 i < static_cast<std::size_t>(bytes_read);
                 ++i)
            {
                if (decoder.feed_byte(buffer[i], frame) &&
                    crsf_channels_decode(frame, channels))
                {
                    guard.on_channels(
                        channels,
                        ManualInputGuard::Clock::now());

                    const auto wheels = mix_manual_sticks(
                        normalize_manual_stick(
                            channels.values[manual_control_config::forward_channel]),
                        normalize_manual_stick(
                            channels.values[manual_control_config::steering_channel]));

                    candidate_command = manual_wheels_to_command(wheels, 120);
                    received_channels = true;
                }
            }
        }

        if (!receiver_drained)
        {
            std::cerr << "ELRS receive drain failed; stopping\n";
            guard.reset();
            runner.set_wheel_command(WheelVelocityCommand{});
            break;
        }

        if (ManualInputGuard::Clock::now() - service_at >= std::chrono::milliseconds{200})
        {
            guard.reset();
            decoder.reset();
            runner.set_wheel_command(WheelVelocityCommand{});
            received_channels = false;
        }

        const bool synchronized_before = runner.is_synchronized();

        if (!synchronized_before)
        {
            guard.reset();
        }

        const bool permitted =
            guard.poll(ManualInputGuard::Clock::now());

        if (!synchronized_before || !permitted)
        {
            runner.set_wheel_command(WheelVelocityCommand{});
        }
        else if (received_channels)
        {
            runner.set_wheel_command(candidate_command);
        }

        runner.poll();

        const bool synchronized = runner.is_synchronized();

        if (!synchronized)
        {
            guard.reset();
            runner.set_wheel_command(WheelVelocityCommand{});
        }

        if (synchronized != previously_synchronized)
        {
            std::cout
                << "STM32 link: "
                << (synchronized ? "SYNCHRONIZED" : "DISCONNECTED")
                << '\n';

            previously_synchronized = synchronized;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    guard.reset();
    runner.set_wheel_command(WheelVelocityCommand{});

    const auto shutdown_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds{2};

    while (std::chrono::steady_clock::now() < shutdown_deadline)
    {
        runner.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return 0;
}